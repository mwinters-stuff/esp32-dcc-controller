#ifndef _WIFI_CONNECTION_H
#define _WIFI_CONNECTION_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <DCCStream.h>
#include <cstring>
#include <esp_timer.h> // For get_absolute_time(), to_ms_since_boot()
#include <lwip/priv/tcp_priv.h>
#include <lwip/tcp.h>
#include <lwip/tcpip.h>
#include <stdarg.h>

// Declare a queue handle
namespace utilities {
extern QueueHandle_t tcp_fail_queue;

class TCPSocketStream : public DCCExController::DCCStream {
private:
  struct tcp_pcb *pcb;
  struct pbuf *recv_buffer;
  bool failed = false;
  err_t err;

  // Heartbeat tracking
  uint64_t heartbeat_sent_time = 0;
  bool awaiting_heartbeat = false;

  static void enqueue_fail_err(err_t fail_err) {
    // Never block from lwIP callback context.
    if (tcp_fail_queue) {
      xQueueSendToBack(tcp_fail_queue, &fail_err, 0);
    }
  }

  static void err_callback(void *arg, err_t err) {
    TCPSocketStream *stream = static_cast<TCPSocketStream *>(arg);
    if (!stream) {
      return;
    }

    stream->pcb = nullptr;
    stream->failed = true;
    stream->err = err;
    enqueue_fail_err(err);
  }

  static err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    TCPSocketStream *stream = static_cast<TCPSocketStream *>(arg);
    if (p == nullptr) {
      // Connection closed by peer.
      printf("Connection closed by remote host\n");
      // Deregister all callbacks and close the PCB before we lose our
      // reference.  Without this the PCB stays alive in lwIP with arg
      // pointing to 'stream'; if the heap reuses that address for a new
      // TCPSocketStream, lwIP can later fire err_callback on the new
      // stream and null its pcb mid-write, causing a LoadProhibited crash.
      tcp_arg(tpcb, nullptr);
      tcp_recv(tpcb, nullptr);
      tcp_err(tpcb, nullptr);
      tcp_close(tpcb);
      stream->pcb = nullptr;
      stream->failed = true;
      stream->err = err;
      if (stream->err == ERR_OK) {
        stream->err = static_cast<err_t>(ERR_CLSD);
      }
      enqueue_fail_err(stream->err);
      return ERR_OK;
    }
    if (err == ERR_OK) {
      if (stream->recv_buffer == nullptr) {
        stream->recv_buffer = p;
      } else {
        pbuf_chain(stream->recv_buffer, p); // Safely chain the new buffer
      }
      uint8_t *data = (uint8_t *)p->payload;
      if (stream->awaiting_heartbeat && data[0] == '<') {
        stream->awaiting_heartbeat = false;
      }
    } else {
      pbuf_free(p);
    }
    return ERR_OK;
  }

public:
  explicit TCPSocketStream(struct tcp_pcb *pcb) : pcb(pcb), recv_buffer(nullptr) {
    // Create a queue (example)

    tcp_recv(pcb, recv_callback);
    tcp_arg(pcb, this);
    tcp_err(pcb, err_callback);

    // Enable keepalive
    pcb->keep_idle = 5000;  // ms
    pcb->keep_intvl = 1000; // ms
    pcb->keep_cnt = 5;
    pcb->so_options |= SOF_KEEPALIVE;
    tcp_keepalive(pcb); // idle, interval, count (values in ms)
  }

  // Check if data is available to read
  int available() const { return recv_buffer ? recv_buffer->tot_len : 0; }

  // Read a single byte from the socket
  int read() {
    LOCK_TCPIP_CORE();
    if (recv_buffer == nullptr) {
      UNLOCK_TCPIP_CORE();
      return -1; // No data
    }
    uint8_t byte = *static_cast<uint8_t *>(recv_buffer->payload);
    pbuf_remove_header(recv_buffer, 1);
    if (recv_buffer->len == 0) {
      struct pbuf *next = recv_buffer->next;
      pbuf_free(recv_buffer);
      recv_buffer = next;
    }
    UNLOCK_TCPIP_CORE();
    return byte;
  }

  // Write a buffer to the socket
  size_t write(const uint8_t *buffer, size_t size) {
    if (failed) {
      return -1; // Already failed
    }
    LOCK_TCPIP_CORE();
    if (pcb == nullptr) {
      UNLOCK_TCPIP_CORE();
      failed = true;
      err_t closed_err = ERR_CLSD;
      enqueue_fail_err(closed_err);
      return -1;
    }
    err_t err = tcp_write(pcb, buffer, size, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
      // tcp_write can internally abort the PCB (e.g. on a MEM error) and
      // synchronously fire err_callback, which sets pcb = nullptr.  Recheck
      // before calling tcp_output to avoid a LoadProhibited on a null pcb.
      if (pcb == nullptr) {
        UNLOCK_TCPIP_CORE();
        failed = true;
        enqueue_fail_err(ERR_CLSD);
        return -1;
      }
      tcp_output(pcb);
      UNLOCK_TCPIP_CORE();
      return size;
    }
    UNLOCK_TCPIP_CORE();
    printf("Error writing to TCP socket: %d\n", err);
    failed = true;
    // Notify main core of TCP failure
    enqueue_fail_err(err);
    return err; // Error
  }

  // No-op for sockets (no explicit flushing needed)
  void flush() {}

  // Send a string with a newline
  void println(const char *format, ...) {
    char buffer[256];

    va_list args; // Declare the variable argument list

    // Initialize the variable argument list
    va_start(args, format);

    // Use vsnprintf to format the string into the buffer
    vsnprintf(buffer, sizeof(buffer), format, args);

    // End the variable argument list
    va_end(args);

    write(reinterpret_cast<const uint8_t *>(buffer), strlen(buffer));
    write(reinterpret_cast<const uint8_t *>("\n"), 1);

    if (strcmp(format, "<#>") == 0) {
      notifyHeartbeatSent();
    }
  }

  // Send a string
  void print(const char *format, ...) {
    char buffer[256];

    va_list args; // Declare the variable argument list

    // Initialize the variable argument list
    va_start(args, format);

    // Use vsnprintf to format the string into the buffer
    vsnprintf(buffer, sizeof(buffer), format, args);

    // End the variable argument list
    va_end(args);

    write(reinterpret_cast<const uint8_t *>(buffer), strlen(buffer));
  }

  // Call this externally when <#> is sent
  void notifyHeartbeatSent() {
    heartbeat_sent_time = esp_timer_get_time();
    awaiting_heartbeat = true;
  }

  // Call this periodically (e.g. in your main loop)
  void checkHeartbeatTimeout() {
    if (awaiting_heartbeat) {
      int64_t elapsed = esp_timer_get_time() - heartbeat_sent_time;
      if (elapsed > 10000000) { // 10 seconds
        printf("Heartbeat timeout\n");
        awaiting_heartbeat = false;
        err_t timeout_err = ERR_TIMEOUT;
        enqueue_fail_err(timeout_err);
      }
    }
  }

  bool isFailed() { return failed; }

  // Destructor to close the socket
  ~TCPSocketStream() {
    LOCK_TCPIP_CORE();
    if (pcb != nullptr) {
      tcp_close(pcb);
      pcb = nullptr;
    }
    if (recv_buffer != nullptr) {
      pbuf_free(recv_buffer);
      recv_buffer = nullptr;
    }
    UNLOCK_TCPIP_CORE();
  }
};

class LoggingStream : public DCCExController::DCCStream {

public:
  explicit LoggingStream(struct tcp_pcb *pcb) {}

  // Check if data is available to read
  int available() const { return 0; }

  // Read a single byte from the socket
  int read() { return 0; }

  // Write a buffer to the socket
  size_t write(const uint8_t *buffer, size_t size) {
    return 0; // Error
  }

  // No-op for sockets (no explicit flushing needed)
  void flush() {}

  // Send a string with a newline
  void println(const char *format, ...) {
    char buffer[256];

    va_list args; // Declare the variable argument list

    // Initialize the variable argument list
    va_start(args, format);

    // Use vsnprintf to format the string into the buffer
    vsnprintf(buffer, sizeof(buffer), format, args);

    // End the variable argument list
    va_end(args);

    printf(buffer);
    printf("\n");
  }

  // Send a string
  void print(const char *format, ...) {
    char buffer[256];

    va_list args; // Declare the variable argument list

    // Initialize the variable argument list
    va_start(args, format);

    // Use vsnprintf to format the string into the buffer
    vsnprintf(buffer, sizeof(buffer), format, args);

    // End the variable argument list
    va_end(args);

    printf(buffer);
  }

  // Destructor to close the socket
  ~LoggingStream() {}
};
} // namespace utilities
#endif