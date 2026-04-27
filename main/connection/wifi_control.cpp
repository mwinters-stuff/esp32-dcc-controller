#include "wifi_control.h"

#include <lwip/ip_addr.h>
#include <lwip/tcp.h>
#include <lwip/tcpip.h>

// #include "../config.h"
#include "definitions.h"
#include "freertos/task.h"
#include "ui/lv_msg.h"
#include "wifi_connection.h"
#include <DCCEXProtocol.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <lwip/apps/mdns.h>

namespace utilities {
extern QueueHandle_t tcp_fail_queue;

static const char *TAG = "WifiControl";

void wifi_loop_task(void *arg) {
  auto self = static_cast<WifiControl *>(arg);
  while (true) {
    self->loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

WifiControl::WifiControl() { init(); }

void WifiControl::init() {
  dccMillis = new ESPDCCMillis();
  xTaskCreate(wifi_loop_task,   // Your task function
              "wifi_loop_task", // Name
              4096,             // Stack size
              this,             // Parameter
              tskIDLE_PRIORITY, // Priority
              nullptr           // Task handle
  );
}

bool WifiControl::connect() { return true; }

err_t WifiControl::tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
  (void)tpcb;
  auto *self = static_cast<WifiControl *>(arg);
  if (self == nullptr) {
    return ERR_VAL;
  }
  self->connectCallbackErr_ = err;
  self->connectCallbackSuccess_ = (err == ERR_OK);
  self->connectCallbackDone_ = true;
  return ERR_OK;
}

void WifiControl::tcp_connect_err_callback(void *arg, err_t err) {
  auto *self = static_cast<WifiControl *>(arg);
  if (self == nullptr) {
    return;
  }
  self->connectCallbackErr_ = err;
  self->connectCallbackSuccess_ = false;
  self->connectCallbackDone_ = true;
}

void WifiControl::failError(err_t err) {
  switch (err) {
  case ERR_MEM:
    ESP_LOGI(TAG, "Out of memory");
    break;
  case ERR_BUF:
    ESP_LOGI(TAG, "Buffer error");
    break;
  case ERR_TIMEOUT:
    ESP_LOGI(TAG, "Timeout");
    break;
  case ERR_RTE:
    ESP_LOGI(TAG, "Routing problem");
    break;
  case ERR_INPROGRESS:
    ESP_LOGI(TAG, "Operation in progress");
    break;
  case ERR_VAL:
    ESP_LOGI(TAG, "Illegal value");
    break;
  case ERR_USE:
    ESP_LOGI(TAG, "Operation already in progress");
    break;
  case ERR_CONN:
    ESP_LOGI(TAG, "Not connected");
    break;
  case ERR_IF:
    ESP_LOGI(TAG, "Low-level netif error");
    break;

  case ERR_ABRT:

    ESP_LOGI(TAG, "Connection aborted");
    break;
  case ERR_RST:
    ESP_LOGI(TAG, "Connection reset");
    break;
  case ERR_CLSD:
    ESP_LOGI(TAG, "Connection closed");
    break;
  default:
    ESP_LOGI(TAG, "Unknown error");
    break;
  }
}

// Function to initiate a connection to the server
void WifiControl::connectToServer(const char *server_ip, uint16_t port) {
  ip_addr_t server_addr;

  if (!ipaddr_aton(server_ip, &server_addr)) {
    printf("Invalid IP address\n");
    return;
  }

  LOCK_TCPIP_CORE();
  struct tcp_pcb *pcb = tcp_new();
  UNLOCK_TCPIP_CORE();
  if (pcb == nullptr) {
    printf("Failed to create PCB\n");
    return;
  }

  connectCallbackDone_ = false;
  connectCallbackSuccess_ = false;
  connectCallbackErr_ = ERR_OK;

  // Attempt to connect
  LOCK_TCPIP_CORE();
  tcp_arg(pcb, this);
  tcp_err(pcb, tcp_connect_err_callback);
  err_t err = tcp_connect(pcb, &server_addr, port, tcp_connected_callback);
  UNLOCK_TCPIP_CORE();
  if (err != ERR_OK) {
    ESP_LOGI(TAG, "Failed to initiate connection: %d\n", err);
    LOCK_TCPIP_CORE();
    enum tcp_state state = pcb->state;
    if (state != CLOSED && state != TIME_WAIT) {
      tcp_abort(pcb);
    }
    UNLOCK_TCPIP_CORE();
    return;
  }

  // Wait for lwIP connect/error callbacks with a 10 second timeout
  ESP_LOGI(TAG, "Connecting to server...");
  currentConnectionState = CONNECTING;
  const uint32_t timeout_ms = 10000;
  uint32_t start_time = millis();
  while (!connectCallbackDone_) {
    uint32_t now = millis(); // Convert to milliseconds
    if (now - start_time > timeout_ms) {
      ESP_LOGI(TAG, "Connection timed out after 10 seconds");
      LOCK_TCPIP_CORE();
      tcp_arg(pcb, nullptr);
      tcp_err(pcb, nullptr);
      enum tcp_state state = pcb->state;
      if (state != CLOSED && state != TIME_WAIT) {
        tcp_abort(pcb);
      }
      UNLOCK_TCPIP_CORE();
      currentConnectionState = DISCONNECTED;
      return;
    }

    // Add a small delay to avoid busy-waiting
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (!connectCallbackSuccess_) {
    ESP_LOGI(TAG, "Connection failed in callback: %d", connectCallbackErr_);
    currentConnectionState = DISCONNECTED;
    return;
  }

  LOCK_TCPIP_CORE();
  ESP_LOGI(TAG, "Connected to server: %s:%d\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port);
  // TCPSocketStream constructor calls tcp_recv/tcp_arg — must stay inside the lock
  stream = new TCPSocketStream(pcb);
  UNLOCK_TCPIP_CORE();

  logStream = new LoggingStream(nullptr);

  dccExProtocol = std::make_shared<DCCExController::DCCEXProtocol>(dccMillis, 600);

  dccExProtocol->setLogStream(logStream);
  dccExProtocol->setDelegate(&dccDelegate);
  dccExProtocol->connect(stream);
  dccExProtocol->setDebug(true);
  dccExProtocol->enableHeartbeat();
  lastGetListsMs = millis();
  currentConnectionState = CONNECTED;

  ESP_LOGI(TAG, "Connection process completed with state: %d", currentConnectionState);
}

void WifiControl::loop() {
  if (dccExProtocol) {
    dccExProtocol->check();
    uint64_t now_ms = millis();
    if (now_ms - lastGetListsMs >= 1000) {
      dccExProtocol->getLists(true, true, true, true);
      lastGetListsMs = now_ms;
    }
  }

  if (stream) {
    stream->checkHeartbeatTimeout();
  }

  err_t err;
  if (xQueueReceive(tcp_fail_queue, &err, 0) == pdTRUE) {
    failError(err);
    disconnect();
    ESP_LOGI(TAG, "Disconnected from server due to error");
    lv_async_call(
        [](void *) {
          lv_msg_send(MSG_DCC_DISCONNECTED, nullptr);
        },
        nullptr);
  }
}

void WifiControl::startConnectToServer(const char *server_ip, uint16_t port) {
  if (currentConnectionState == CONNECTING || currentConnectionState == CONNECTED) {
    ESP_LOGI(TAG, "Ignoring connect request; current state=%d", currentConnectionState);
    return;
  }
  auto *args = new ConnectTaskArgs{this, server_ip, port};
  xTaskCreate(&WifiControl::connect_task, "connect_task", 4096, args, tskIDLE_PRIORITY, nullptr);
}

void WifiControl::disconnect() {
  if (dccExProtocol) {
    dccExProtocol->disconnect();
    dccExProtocol = nullptr;
  }
  if (stream) {
    delete stream;
    stream = nullptr;
  }
  if (logStream) {
    delete logStream;
    logStream = nullptr;
  }
  currentConnectionState = DISCONNECTED;
  ESP_LOGI(TAG, "Disconnected from server");
}

void WifiControl::connect_task(void *arg) {
  auto *args = static_cast<ConnectTaskArgs *>(arg);
  if (args && args->self) {
    args->self->connectToServer(args->server_ip.c_str(), args->port);
  }
  delete args;
  vTaskDelete(nullptr);
}

}; // namespace utilities