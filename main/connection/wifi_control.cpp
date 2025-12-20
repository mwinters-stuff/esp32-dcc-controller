#include "wifi_control.h"

#include <lwip/ip_addr.h>
#include <lwip/tcp.h>

// #include "../config.h"
#include "lwip/apps/mdns.h"
#include "wifi_connection.h"
#include <DCCEXProtocol.h>
#include <esp_log.h>
#include <esp_timer.h>

// Define the static instance variable
// std::shared_ptr<WifiControl> WifiControl::instance = nullptr;
extern QueueHandle_t tcp_fail_queue;

static const char *TAG = "WifiControl";

// Static timer handle
// static esp_timer_handle_t wifi_loop_timer = nullptr;

// Timer callback
// static void wifi_loop_timer_callback(void *arg) {
//   auto self = static_cast<WifiControl *>(arg);
//   if (self) {
//     self->loop();
//   }
// }

void wifi_loop_task(void *arg) {
  auto self = static_cast<WifiControl *>(arg);
  while (true) {
    self->loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

WifiControl::WifiControl() { init(); }

void WifiControl::init() {
  xTaskCreatePinnedToCore(wifi_loop_task,   // Your task function
                          "wifi_loop_task", // Name
                          4096,             // Stack size
                          this,             // Parameter
                          5,                // Priority
                          nullptr,          // Task handle
                          1                 // Core 1
  );
  // if (!wifi_loop_timer) {
  //   esp_timer_create_args_t timer_args = {
  //       .callback = &wifi_loop_timer_callback,
  //       .arg = this,
  //       .dispatch_method = ESP_TIMER_TASK,
  //       .name = "wifi_loop",
  //       .skip_unhandled_events = false,
  //   };
  //   esp_timer_create(&timer_args, &wifi_loop_timer);
  //   esp_timer_start_periodic(wifi_loop_timer, 50 * 1000); // 50ms in microseconds
  // }
}

bool WifiControl::connect() { return true; }

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

  struct tcp_pcb *pcb = tcp_new();
  if (pcb == nullptr) {
    printf("Failed to create PCB\n");
    return;
  }

  // Attempt to connect
  err_t err = tcp_connect(pcb, &server_addr, port, nullptr);
  if (err != ERR_OK) {
    ESP_LOGI(TAG, "Failed to initiate connection: %d\n", err);
    tcp_close(pcb);
    return;
  }

  // Poll for connection status with a 20 second timeout
  ESP_LOGI(TAG, "Connecting to server...");
  currentConnectionState = CONNECTING;
  const uint32_t timeout_ms = 10000;
  uint32_t start_time = esp_timer_get_time() / 1000; // Convert to milliseconds;
  while (currentConnectionState == CONNECTING) {
    // cyw43_arch_poll();

    // Check the connection state
    if (pcb->state == ESTABLISHED) {
      ESP_LOGI(TAG, "Connected to server: %s:%d\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port);

      stream = new TCPSocketStream(pcb);
      logStream = new LoggingStream(nullptr);
      dccExProtocol = std::make_shared<DCCExController::DCCEXProtocol>();

      dccExProtocol->setLogStream(logStream);
      dccExProtocol->setDelegate(&dccDelegate);
      dccExProtocol->connect(stream);
      dccExProtocol->enableHeartbeat();
      currentConnectionState = CONNECTED;
    } else if (pcb->state == CLOSED || pcb->state == TIME_WAIT || pcb->state == FIN_WAIT_1 ||
               pcb->state == FIN_WAIT_2) {
      printf("Connection failed or closed\n");
      ESP_LOGI(TAG, "failed");
      tcp_close(pcb);
      currentConnectionState = DISCONNECTED;
      return;
    }

    // Timeout check
    uint32_t now = esp_timer_get_time() / 1000; // Convert to milliseconds
    if (now - start_time > timeout_ms) {
      ESP_LOGI(TAG, "Connection timed out after 10 seconds");
      tcp_close(pcb);
      return;
    }

    // Add a small delay to avoid busy-waiting
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG, "Connection process completed with state: %d", currentConnectionState);
}

void WifiControl::loop() {
  if (dccExProtocol) {
    dccExProtocol->check();
    dccExProtocol->getLists(true, true, true, true);
  }

  if (stream) {
    stream->checkHeartbeatTimeout();
  }

  err_t err;
  if (xQueueReceive(tcp_fail_queue, &err, 0) == pdTRUE) {
    failError(err);
    disconnect();
    ESP_LOGI(TAG, "Disconnected from server due to error");
    // displayControls->showConnectionFailed();
    // displayControls->showDCCConnectionMenu();
  }
}

void WifiControl::startConnectToServer(const char *server_ip, uint16_t port) {
  auto *args = new ConnectTaskArgs{this, server_ip, port};
  xTaskCreatePinnedToCore(&WifiControl::connect_task, "connect_task", 4096, args, 5, nullptr,
                          1 // Core 1
  );
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