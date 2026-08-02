#include "LGFX_ILI9488_S3.hpp"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "display/DCCMenu.h"
#include "display/DisplayManager.h"
#include "display/FirstScreen.h"
#include "display/ManualCalibration.h"
#include "display/MessageBox.h"
#include "display/WaitingScreen.h"
#include "display/WifiConnectScreen.h"
#include "ui/LvglTheme.h"
#include "utilities/PowerSettings.h"
#include "utilities/RotaryEncoder.h"
#include "utilities/Screenshot.h"
#include "utilities/WifiHandler.h"
#include <LovyanGFX.hpp>
#include <atomic>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <memory>
#include <nvs_flash.h>
#include <nvs_handle.hpp>
#include <string>

static const char *TAG = "main";

namespace {

void return_to_main_screen(void *) { display::FirstScreen::instance()->showScreen(); }

} // namespace

static uint32_t lv_tick_ms_cb() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }

// --- LVGL Variables ---
static lv_display_t *lvgl_disp = nullptr;

// --- Display sleep tracking ---
static std::atomic_bool displaySleeping{false};
static std::atomic_bool pendingDccDisconnectPopup{false};
static std::atomic_bool reconnectDccAfterWake{false};
static std::atomic_bool reconnectDccInProgress{false};
static std::atomic_bool reconnectWifiAfterWake{false};
static std::atomic_bool networkSleepDisconnectApplied{false};
static std::atomic_uint32_t displayOffTimeoutMs{utilities::kDefaultDisplayOffMinutes * 60000UL};
static std::atomic_uint32_t sleepDisconnectTimeoutMs{utilities::kDefaultSleepDisconnectMinutes * 60000UL};

// Owns the WaitingScreen shown during a post-wake DCC reconnect attempt.
static std::shared_ptr<display::WaitingScreen> reconnectWaitScreen_;

constexpr uint32_t ACTIVE_LOOP_DELAY_MS = 10;
constexpr uint32_t SLEEP_LOOP_DELAY_MS = 100;

void load_power_settings_from_nvs() {
  auto settings = utilities::loadPowerSettings();
  const uint32_t displayMs = static_cast<uint32_t>(settings.displayOffMinutes) * 60000UL;
  const uint32_t sleepMs = static_cast<uint32_t>(settings.sleepDisconnectMinutes) * 60000UL;
  displayOffTimeoutMs.store(displayMs);
  sleepDisconnectTimeoutMs.store(sleepMs);
  ESP_LOGI(TAG, "Power settings loaded: display_off=%u min sleep_disconnect=%u min",
           static_cast<unsigned>(settings.displayOffMinutes), static_cast<unsigned>(settings.sleepDisconnectMinutes));
}

bool load_saved_dcc_endpoint(std::string &ip, uint16_t &port) {
  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_DCC, NVS_READONLY, &err);
  if (err != ESP_OK || !handle) {
    return false;
  }

  uint8_t saved = 0;
  err = handle->get_item(NVS_DCC_SAVED, saved);
  if (err != ESP_OK || saved == 0) {
    return false;
  }

  char ip_buf[16] = {0};
  uint16_t saved_port = 0;
  err = handle->get_string(NVS_DCC_IP, ip_buf, sizeof(ip_buf));
  if (err != ESP_OK || ip_buf[0] == '\0') {
    return false;
  }

  err = handle->get_item(NVS_DCC_PORT, saved_port);
  if (err != ESP_OK || saved_port == 0) {
    return false;
  }

  ip = ip_buf;
  port = saved_port;
  return true;
}

void maybe_start_dcc_reconnect() {
  if (!reconnectDccAfterWake.load() || reconnectDccInProgress.load()) {
    return;
  }

  if (displaySleeping.load()) {
    return;
  }

  auto wifiHandler = utilities::WifiHandler::instance();
  if (!wifiHandler->isConnected()) {
    return;
  }

  auto wifiControl = utilities::WifiControl::instance();
  if (wifiControl->connectionState() == utilities::WifiControl::CONNECTING ||
      wifiControl->connectionState() == utilities::WifiControl::CONNECTED) {
    reconnectDccAfterWake.store(false);
    reconnectDccInProgress.store(false);
    return;
  }

  // Prefer the in-memory active endpoint set when the connection succeeded;
  // fall back to the user-saved NVS entry only if no active endpoint exists.
  std::string ip;
  uint16_t port = 0;
  if (wifiControl->hasActiveEndpoint()) {
    ip = wifiControl->activeServerIp();
    port = wifiControl->activeServerPort();
    ESP_LOGI(TAG, "Reconnecting to last-active DCC endpoint %s:%u", ip.c_str(), static_cast<unsigned>(port));
  } else if (!load_saved_dcc_endpoint(ip, port)) {
    ESP_LOGW(TAG, "No DCC endpoint available for reconnect (no active or saved endpoint)");
    reconnectDccAfterWake.store(false);
    reconnectDccInProgress.store(false);
    return;
  } else {
    ESP_LOGI(TAG, "Reconnecting to NVS-saved DCC endpoint %s:%u", ip.c_str(), static_cast<unsigned>(port));
  }

  // Pre-configure DCCMenu with the target server so that when WaitingScreen's
  // success handler calls parent->showScreen() it already has the right address.
  auto dccMenu = display::DCCMenu::instance();
  dccMenu->setConnectedServer(ip, static_cast<int>(port), ip);

  // Show a reconnecting indicator. DCCMenu is passed as the parent so that on
  // success WaitingScreen navigates directly into it.
  reconnectWaitScreen_ = std::make_shared<display::WaitingScreen>();
  reconnectWaitScreen_->setLabel("Reconnecting to DCC...");
  reconnectWaitScreen_->setSubLabel(ip);
  reconnectWaitScreen_->showScreen(dccMenu);

  ESP_LOGI(TAG, "Attempting DCC reconnect to %s:%u after wake", ip.c_str(), static_cast<unsigned>(port));
  reconnectDccInProgress.store(true);
  wifiControl->startConnectToServer(ip.c_str(), port);

  // Spawn a lightweight task that polls for the connection result and sends the
  // appropriate message so WaitingScreen can handle success/failure UI.
  struct PollArgs {
    utilities::WifiControl *wifiControl;
    std::string ip;
    uint16_t port;
  };
  auto *pollArgs = new PollArgs{wifiControl.get(), ip, port};
  xTaskCreate(
      [](void *arg) {
        auto *args = static_cast<PollArgs *>(arg);
        utilities::WifiControl::connection_state state;
        do {
          vTaskDelay(pdMS_TO_TICKS(50));
          state = args->wifiControl->connectionState();
        } while (state == utilities::WifiControl::CONNECTING);

        if (state == utilities::WifiControl::CONNECTED) {
          ESP_LOGI("main", "DCC reconnect to %s:%u succeeded", args->ip.c_str(), static_cast<unsigned>(args->port));
          lv_async_call([](void *) { lv_msg_send(MSG_DCC_CONNECTION_SUCCESS, NULL); }, nullptr);
        } else {
          ESP_LOGW("main", "DCC reconnect to %s:%u failed", args->ip.c_str(), static_cast<unsigned>(args->port));
          lv_async_call([](void *) { lv_msg_send(MSG_DCC_CONNECTION_FAILED, NULL); }, nullptr);
        }
        delete args;
        vTaskDelete(nullptr);
      },
      "reconnect_poll", 4096, pollArgs, tskIDLE_PRIORITY, nullptr);
}

// --- FADE EFFECT ---
void fade_brightness(uint8_t from, uint8_t to, int duration_ms) {
  const int steps = 25; // smoother
  float step_time = (float)duration_ms / steps;
  float delta = (to - from) / (float)steps;

  for (int i = 0; i <= steps; ++i) {
    uint8_t val = static_cast<uint8_t>(from + delta * i);
    DisplayManager::gfx.setBrightness(val);
    vTaskDelay(pdMS_TO_TICKS(step_time));
  }
}

// --- Display control helpers ---
void display_set_sleep(bool sleep) {
  if (sleep) {
    ESP_LOGI(TAG, "Display sleeping...");
    fade_brightness(255, 0, 800); // fade out
  } else {
    ESP_LOGI(TAG, "Display waking...");
    fade_brightness(0, 255, 600); // fade in
  }
}

void wake_display_if_sleeping(lv_display_t *disp) {
  if (disp == nullptr) {
    return;
  }

  if (displaySleeping.load()) {
    lv_display_trigger_activity(disp);
    if (displaySleeping.exchange(false)) {
      display_set_sleep(false);
    }

    if (networkSleepDisconnectApplied.exchange(false)) {
      utilities::WifiHandler::instance()->setAutoReconnectEnabled(true);
      if (reconnectWifiAfterWake.exchange(false)) {
        const esp_err_t reconnectErr = esp_wifi_connect();
        if (reconnectErr != ESP_OK && reconnectErr != ESP_ERR_WIFI_STATE) {
          ESP_LOGE(TAG, "Failed to reconnect WiFi on wake: %s", esp_err_to_name(reconnectErr));
        }
      }
      maybe_start_dcc_reconnect();
    }
  }
}

void apply_sleep_disconnect_if_needed() {
  if (!displaySleeping.load() || networkSleepDisconnectApplied.load() || lvgl_disp == nullptr) {
    return;
  }

  const uint32_t inactive_ms = lv_display_get_inactive_time(lvgl_disp);
  if (inactive_ms <= sleepDisconnectTimeoutMs.load()) {
    return;
  }

  ESP_LOGI(TAG, "Sleep/disconnect timeout reached; disconnecting WiFi and DCC");

  auto wifiControl = utilities::WifiControl::instance();
  const auto dccState = wifiControl->connectionState();
  const bool shouldReconnectDcc =
      (dccState == utilities::WifiControl::CONNECTED || dccState == utilities::WifiControl::CONNECTING);

  if (shouldReconnectDcc) {
    reconnectDccAfterWake.store(true);
    reconnectDccInProgress.store(false);
    wifiControl->disconnect();
  }

  if (utilities::WifiHandler::instance()->isConnected()) {
    utilities::WifiHandler::instance()->setAutoReconnectEnabled(false);
    const esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_STATE) {
      ESP_LOGE(TAG, "Failed to disconnect WiFi for sleep: %s", esp_err_to_name(err));
    } else {
      reconnectWifiAfterWake.store(true);
    }
  }

  networkSleepDisconnectApplied.store(true);
}

void idle_for_power_saving() {
  if (!displaySleeping.load()) {
    vTaskDelay(pdMS_TO_TICKS(ACTIVE_LOOP_DELAY_MS));
    return;
  }

  // Keep links alive until sleep/disconnect timeout is reached.
  if (!networkSleepDisconnectApplied.load() && utilities::WifiHandler::instance()->isConnected()) {
    vTaskDelay(pdMS_TO_TICKS(SLEEP_LOOP_DELAY_MS));
    return;
  }

  const esp_err_t wakeCfgErr = esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(SLEEP_LOOP_DELAY_MS) * 1000ULL);
  if (wakeCfgErr != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure light sleep wake timer: %s", esp_err_to_name(wakeCfgErr));
    vTaskDelay(pdMS_TO_TICKS(SLEEP_LOOP_DELAY_MS));
    return;
  }

  const esp_err_t sleepErr = esp_light_sleep_start();
  if (sleepErr != ESP_OK) {
    ESP_LOGE(TAG, "esp_light_sleep_start failed: %s", esp_err_to_name(sleepErr));
    vTaskDelay(pdMS_TO_TICKS(SLEEP_LOOP_DELAY_MS));
  }
}

void app_note_user_activity() {
  if (lvgl_disp == nullptr) {
    return;
  }

  lv_async_call(
      [](void *d) {
        auto *display = static_cast<lv_display_t *>(d);
        wake_display_if_sleeping(display);
        lv_display_trigger_activity(display);
      },
      lvgl_disp);
}

// --- Touchpad Read Callback ---
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;

  if (utilities::isInputPausedForCapture()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int32_t x = 0;
  int32_t y = 0;
  bool touched = DisplayManager::gfx.getTouch(&x, &y);

  constexpr bool FLIP_X = false;
  constexpr bool FLIP_Y = true;
  constexpr bool SWAP_XY = false;

  if (touched) {
    int32_t tx = x;
    int32_t ty = y;

    if (SWAP_XY)
      std::swap(tx, ty);
    if (FLIP_X)
      tx = static_cast<int32_t>(DisplayManager::gfx.screenWidth - tx - 1);
    if (FLIP_Y)
      ty = static_cast<int32_t>(DisplayManager::gfx.screenHeight - ty - 1);

    data->point.x = tx;
    data->point.y = ty;
    data->state = LV_INDEV_STATE_PRESSED;

    lv_display_trigger_activity(lvgl_disp);

    if (displaySleeping.load()) {
      wake_display_if_sleeping(lvgl_disp);
      data->state = LV_INDEV_STATE_RELEASED;
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// --- App setup ---
auto manualCalibration = display::ManualCalibration::instance();
auto firstScreen = display::FirstScreen::instance();

void setup() {
  ESP_LOGI(TAG, "ES32 DCC Controller");

  display::calibrateState calibrated = manualCalibration->loadCalibrationFromNVS();

  ESP_LOGI(TAG, "Init Display");

  DisplayManager::gfx.begin();
  DisplayManager::gfx.setRotation(0);
  DisplayManager::gfx.fillScreen(TFT_BLACK);
  DisplayManager::gfx.setBrightness(255);

  if (calibrated == display::calibrateState::calibrate) {
    manualCalibration->calibrate();
    return;
  }

  lv_init();
  lv_tick_set_cb(lv_tick_ms_cb);
  auto buffer_size = DisplayManager::bufferSize();
  lv_color_t *buf1 = new lv_color_t[buffer_size];
  lv_color_t *buf2 = new lv_color_t[buffer_size];
  lvgl_disp = lv_display_create(DisplayManager::gfx.screenWidth, DisplayManager::gfx.screenHeight);
  if (!lvgl_disp) {
    ESP_LOGE(TAG, "lv_display_create failed");
    return;
  }
  lv_display_set_default(lvgl_disp);
  lv_display_set_buffers(lvgl_disp, buf1, buf2, buffer_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvgl_disp, DisplayManager::disp_flush);

  lv_indev_t *indev = lv_indev_create();
  if (!indev) {
    ESP_LOGE(TAG, "lv_indev_create failed");
    return;
  }
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(indev, lvgl_disp);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  ESP_LOGI(TAG, "Touch indev registered: %p", indev);

  auto theme = std::make_shared<ui::LvglTheme>("Default");
  ui::LvglTheme::setActive(theme);

  load_power_settings_from_nvs();

#if CONFIG_ROTARY_ENCODER_ENABLE
  utilities::RotaryEncoder::instance()->init(
      static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_GPIO_A), static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_GPIO_B),
      CONFIG_ROTARY_ENCODER_DEFAULT_DIRECTION == 1,
#if CONFIG_ROTARY_ENCODER_SW_ENABLE
      true, static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_GPIO_SW), CONFIG_ROTARY_ENCODER_SW_ACTIVE_LEVEL
#else
      false, GPIO_NUM_NC, 0
#endif
  );
  utilities::RotaryEncoder::instance()->setActivityCallback(
      [](void *) {
        app_note_user_activity();
      },
      nullptr);
#endif

  lv_msg_subscribe(
      MSG_WIFI_FAILED,
      [](lv_msg_t *msg) {
        auto *payload = static_cast<const WifiFailedPayload *>(lv_msg_get_payload(msg));
        if (payload != nullptr && payload->suppressGlobalPopup) {
          ESP_LOGI(TAG, "Skipping global WiFi failure popup (local handler exception)");
          return;
        }
        lv_async_call(
            [](void *) {
              display::showMessageBox("WiFi Failed", "WiFi connection was lost.", display::MessageBoxState::Error,
                                      return_to_main_screen, nullptr);
            },
            nullptr);
      },
      nullptr);

  lv_msg_subscribe(
      MSG_DCC_DISCONNECTED,
      [](lv_msg_t *) {
        ESP_LOGI(TAG, "Received MSG_DCC_DISCONNECTED");
        if (displaySleeping.load()) {
          reconnectDccAfterWake.store(true);
          reconnectDccInProgress.store(false);
          return;
        }
        pendingDccDisconnectPopup.store(true);
      },
      nullptr);

  lv_msg_subscribe(
      MSG_WIFI_CONNECTED,
      [](lv_msg_t *) {
        maybe_start_dcc_reconnect();
      },
      nullptr);

  lv_msg_subscribe(
      MSG_DCC_CONNECTION_SUCCESS,
      [](lv_msg_t *) {
        reconnectDccAfterWake.store(false);
        reconnectDccInProgress.store(false);
        // WaitingScreen owns navigation to DCCMenu on success; release our ref
        // after LVGL has processed the current event dispatch round.
        lv_async_call([](void *) { reconnectWaitScreen_.reset(); }, nullptr);
      },
      nullptr);

  lv_msg_subscribe(
      MSG_DCC_CONNECTION_FAILED,
      [](lv_msg_t *) {
        if (reconnectDccInProgress.exchange(false)) {
          reconnectDccAfterWake.store(false);
          // WaitingScreen handles the failure UI (error box → FirstScreen);
          // release our ref after LVGL has processed the event dispatch round.
          lv_async_call([](void *) { reconnectWaitScreen_.reset(); }, nullptr);
        }
      },
      nullptr);

  lv_msg_subscribe(
      MSG_POWER_SETTINGS_UPDATED,
      [](lv_msg_t *) {
        load_power_settings_from_nvs();
      },
      nullptr);

  lv_msg_subscribe(
      MSG_TAKE_SCREENSHOT,
      [](lv_msg_t *) {
        lv_async_call(
            [](void *) {
              if (!utilities::saveActiveScreenScreenshot()) {
                ESP_LOGE(TAG, "Screenshot capture failed");
              }
            },
            nullptr);
      },
      nullptr);

  switch (calibrated) {
  case display::calibrateState::calibrated:
    xTaskCreate(
        [](void *parameters) {
          (void)parameters;
          vTaskDelay(pdMS_TO_TICKS(2000));
          utilities::WifiHandler::instance()->init_wifi();
          vTaskDelete(nullptr);
        },
        "wifi_init_del", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    firstScreen->showScreen();
    break;
  case display::calibrateState::notCalibrated:
  case display::calibrateState::showScreen:
    manualCalibration->showScreen();
    break;
  default:
    ESP_LOGI(TAG, "Calibrated is not correct.");
    manualCalibration->showScreen();
    break;
  }

  ESP_LOGI(TAG, "Setup complete. UI should be visible.");
}

extern "C" void app_main() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  } else {
    ESP_ERROR_CHECK(ret);
  }

  setup();

  while (true) {
    lv_timer_handler();
    apply_sleep_disconnect_if_needed();
    idle_for_power_saving();
    maybe_start_dcc_reconnect();

    if (pendingDccDisconnectPopup.exchange(false)) {
      ESP_LOGI(TAG, "Showing DCC disconnected message box");
      wake_display_if_sleeping(lvgl_disp);
      lv_display_trigger_activity(lvgl_disp);
      display::showMessageBox("DCC Disconnected", "Connection to the DCC server was lost.",
                              display::MessageBoxState::Warning, return_to_main_screen, nullptr);
    }

    if (!displaySleeping.load()) {
      uint32_t inactive_ms = lv_display_get_inactive_time(lvgl_disp);
      if (inactive_ms > displayOffTimeoutMs.load()) {
        display_set_sleep(true);
        displaySleeping.store(true);
        networkSleepDisconnectApplied.store(false);
        reconnectWifiAfterWake.store(false);
      }
    }
  }
}
