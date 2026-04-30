/**
 * @file wifi_control.cpp
 * @brief WifiControl — manages the WiThrottle TCP connection lifecycle.
 *
 * Opens a raw lwIP TCP socket to a DCC-EX WiThrottle server, feeds incoming
 * data to DCCEXProtocol, and publishes lv_msg events for connection state
 * changes. A dedicated FreeRTOS task (`wifi_loop_task`) drives the protocol
 * loop; access to shared state is serialised with a FreeRTOS mutex.
 */
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

// FreeRTOS task body: calls WifiControl::loop() in a tight loop until the
// WifiControl instance is destroyed.
void wifi_loop_task(void *arg) {
  auto self = static_cast<WifiControl *>(arg);
  while (true) {
    self->loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// Constructor: calls init() to allocate the mutex and start the loop task.
WifiControl::WifiControl() { init(); }

// Creates the state mutex and spawns wifi_loop_task.
void WifiControl::init() {
  dccMillis = new ESPDCCMillis();
  if (stateMutex_ == nullptr) {
    stateMutex_ = xSemaphoreCreateMutex();
  }
  xTaskCreate(wifi_loop_task,   // Your task function
              "wifi_loop_task", // Name
              4096,             // Stack size
              this,             // Parameter
              tskIDLE_PRIORITY, // Priority
              nullptr           // Task handle
  );
}

// Placeholder — TCP connection is initiated by connectToServer(); always
// returns true.
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

// lwIP TCP error callback: fires when the connection is aborted by the remote
// end or by a network error. Delegates to failError().
void WifiControl::tcp_connect_err_callback(void *arg, err_t err) {
  auto *self = static_cast<WifiControl *>(arg);
  if (self == nullptr) {
    return;
  }
  self->connectCallbackErr_ = err;
  self->connectCallbackSuccess_ = false;
  self->connectCallbackDone_ = true;
}

// Publishes MSG_DCC_CONNECTION_FAILED with the lwIP error code and tears down
// any partially-open socket.
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
// Opens a non-blocking lwIP TCP socket and initiates a connection to the given
// server. Registers the recv, sent and error callbacks on the pcb.
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
  auto *newStream = new TCPSocketStream(pcb);
  UNLOCK_TCPIP_CORE();

  // Build the protocol fully in local variables first.  If we assigned
  // dccExProtocol before calling connect(stream), wifi_loop_task could pick
  // up the non-null pointer and call check() on a protocol that has no stream
  // yet, leading to a null-stream write crash.
  auto *newLogStream = new LoggingStream(nullptr);
  auto newProtocol = std::make_shared<DCCExController::DCCEXProtocol>(dccMillis, 600);
  newProtocol->setLogStream(newLogStream);
  newProtocol->setDelegate(&dccDelegate);
  newProtocol->connect(newStream);
  newProtocol->setDebug(true);
  newProtocol->enableHeartbeat();

  // Drain any stale errors left in the queue by the previous connection so
  // they cannot trigger an immediate disconnect of the new session.
  err_t staleErr;
  while (xQueueReceive(tcp_fail_queue, &staleErr, 0) == pdTRUE) {
  }

  // Publish atomically under stateMutex_ so loop() never sees a partially
  // initialised state.
  if (xSemaphoreTake(stateMutex_, portMAX_DELAY) == pdTRUE) {
    stream = newStream;
    logStream = newLogStream;
    dccExProtocol = newProtocol;
    lastGetListsMs = millis();
    currentConnectionState = CONNECTED;
    xSemaphoreGive(stateMutex_);
  }

  ESP_LOGI(TAG, "Connection process completed with state: %d", currentConnectionState);
}

// Main protocol loop: takes the state mutex and calls
// dccExProtocol->loop() to process any inbound WiThrottle data. Must be
// called repeatedly from wifi_loop_task.
void WifiControl::loop() {
  if (stateMutex_ != nullptr && xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
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

    xSemaphoreGive(stateMutex_);
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

// Thread-safe entry point for screens: copies the address/port and spawns a
// short-lived FreeRTOS task that calls connectToServer on the lwIP thread.
void WifiControl::startConnectToServer(const char *server_ip, uint16_t port) {
  if (currentConnectionState == CONNECTING || currentConnectionState == CONNECTED) {
    ESP_LOGI(TAG, "Ignoring connect request; current state=%d", currentConnectionState);
    return;
  }
  auto *args = new ConnectTaskArgs{this, server_ip, port};
  xTaskCreate(&WifiControl::connect_task, "connect_task", 4096, args, tskIDLE_PRIORITY, nullptr);
}

// Closes the TCP socket, destroys the protocol/stream objects and publishes
// MSG_DCC_DISCONNECTED. Serialised under stateMutex_.
void WifiControl::disconnect() {
  if (stateMutex_ == nullptr) {
    return;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "disconnect skipped: state mutex unavailable");
    return;
  }

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

  xSemaphoreGive(stateMutex_);
  ESP_LOGI(TAG, "Disconnected from server");
}

// Sends a turnout throw/close command while holding the shared state mutex so
// protocol writes cannot race with disconnect teardown.
bool WifiControl::setTurnoutThrown(int turnoutId, bool thrown) {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "setTurnoutThrown skipped: state mutex unavailable");
    return false;
  }

  bool ok = false;
  if (currentConnectionState == CONNECTED && dccExProtocol && stream) {
    if (thrown) {
      dccExProtocol->throwTurnout(turnoutId);
    } else {
      dccExProtocol->closeTurnout(turnoutId);
    }
    ok = true;
  } else {
    ESP_LOGW(TAG, "setTurnoutThrown ignored: not connected");
  }

  xSemaphoreGive(stateMutex_);
  return ok;
}

// Starts a route while holding the shared state mutex so command writes cannot
// race with disconnect teardown.
bool WifiControl::startRoute(int routeId) {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "startRoute skipped: state mutex unavailable");
    return false;
  }

  bool ok = false;
  if (currentConnectionState == CONNECTED && dccExProtocol && stream) {
    dccExProtocol->startRoute(routeId);
    ok = true;
  } else {
    ESP_LOGW(TAG, "startRoute ignored: not connected");
  }

  xSemaphoreGive(stateMutex_);
  return ok;
}

// Pauses or resumes routes while holding the shared state mutex.
bool WifiControl::setRoutesPaused(bool paused) {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "setRoutesPaused skipped: state mutex unavailable");
    return false;
  }

  bool ok = false;
  if (currentConnectionState == CONNECTED && dccExProtocol && stream) {
    if (paused) {
      dccExProtocol->pauseRoutes();
    } else {
      dccExProtocol->resumeRoutes();
    }
    ok = true;
  } else {
    ESP_LOGW(TAG, "setRoutesPaused ignored: not connected");
  }

  xSemaphoreGive(stateMutex_);
  return ok;
}

// Sends a turntable move command while holding the shared state mutex.
bool WifiControl::rotateTurntableToIndex(int turntableId, int indexId) {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "rotateTurntableToIndex skipped: state mutex unavailable");
    return false;
  }

  bool ok = false;
  if (currentConnectionState == CONNECTED && dccExProtocol && stream) {
    dccExProtocol->rotateTurntable(turntableId, indexId);
    ok = true;
  } else {
    ESP_LOGW(TAG, "rotateTurntableToIndex ignored: not connected");
  }

  xSemaphoreGive(stateMutex_);
  return ok;
}

// Sends the raw turntable reverse command while holding the shared state
// mutex.
bool WifiControl::sendTurntableReverseCommand(int turntableId) {
  if (stateMutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTake(stateMutex_, pdMS_TO_TICKS(250)) != pdTRUE) {
    ESP_LOGW(TAG, "sendTurntableReverseCommand skipped: state mutex unavailable");
    return false;
  }

  bool ok = false;
  if (currentConnectionState == CONNECTED && dccExProtocol && stream) {
    char command[24];
    snprintf(command, sizeof(command), "I %d 0 18", turntableId);
    dccExProtocol->sendCommand(command);
    ok = true;
  } else {
    ESP_LOGW(TAG, "sendTurntableReverseCommand ignored: not connected");
  }

  xSemaphoreGive(stateMutex_);
  return ok;
}

// FreeRTOS task spawned by startConnectToServer: resolves the IP and calls
// connectToServer, then deletes itself.
void WifiControl::connect_task(void *arg) {
  auto *args = static_cast<ConnectTaskArgs *>(arg);
  if (args && args->self) {
    args->self->connectToServer(args->server_ip.c_str(), args->port);
  }
  delete args;
  vTaskDelete(nullptr);
}

}; // namespace utilities