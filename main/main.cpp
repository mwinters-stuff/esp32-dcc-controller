#include "definitions.h"
#include "display/DisplayManager.h"
#include "display/FirstScreen.h"
#include "display/ManualCalibration.h"
#include "display/WifiConnectScreen.h"
#include "millis.h"
#include "ui/LvglTheme.h"
#include "utilities/WifiHandler.h"
#include <LGFX_ILI9488_S3.hpp>
#include <LovyanGFX.hpp>
#include <atomic>
#include <chrono>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <memory>
#include <nvs_flash.h>
#include <string>

static const char *TAG = "main";

// --- LVGL Variables ---
static lv_display_t *lvgl_disp = nullptr;

// --- Inactivity tracking ---
static uint64_t lastActivityTime = 0;
static bool displaySleeping = false;
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 2 * 60 * 1000; // 2 minutes

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

// --- Touchpad Read Callback ---
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
  int32_t x = 0;
  int32_t y = 0;
  bool touched = DisplayManager::gfx.getTouch(&x, &y);

  // Adjust these to match your hardware / rotation
  // Try FLIP_Y = true to fix upside-down touches
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

    if (displaySleeping) {
      display_set_sleep(false);
      displaySleeping = false;
      data->state = LV_INDEV_STATE_RELEASED;
      lastActivityTime = millis();
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
  DisplayManager::gfx.setBrightness(255); // ensure display on at start

  if (calibrated == display::calibrateState::calibrate) {
    manualCalibration->calibrate();
    return;
  }

  lv_init();
  auto buffer_size = DisplayManager::bufferSize();
  lv_color_t *buf1 = new lv_color_t[buffer_size];
  lv_color_t *buf2 = new lv_color_t[buffer_size];
  lvgl_disp = lv_display_create(DisplayManager::gfx.screenWidth, DisplayManager::gfx.screenHeight);
  lv_display_set_buffers(lvgl_disp, buf1, buf2, buffer_size * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvgl_disp, DisplayManager::disp_flush);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);
  ESP_LOGI(TAG, "Touch indev registered: %p", indev);

  auto theme = std::make_shared<ui::LvglTheme>("Default");
  ui::LvglTheme::setActive(theme);

  // Global handler: whenever the DCC server drops, return to the home screen
  // regardless of which screen is currently active.
  lv_msg_subscribe(
      MSG_DCC_DISCONNECTED,
      [](lv_msg_t *) {
        display::FirstScreen::instance()->showScreen();
      },
      nullptr);

  switch (calibrated) {
  case display::calibrateState::calibrated:
    xTaskCreate(
        [](void *parameters) {
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

  lastActivityTime = millis();
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
    vTaskDelay(pdMS_TO_TICKS(10));

    // --- Inactivity check ---
    uint32_t now = millis();
    if (!displaySleeping && (now - lastActivityTime > INACTIVITY_TIMEOUT_MS)) {
      display_set_sleep(true);
      displaySleeping = true;
    }
  }
}
