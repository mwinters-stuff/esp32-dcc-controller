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
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_color_t *buf2;

// --- Inactivity tracking ---
static uint64_t lastActivityTime = 0;
static bool displaySleeping = false;
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 2 * 60 * 1000; // 2 minutes

// --- FADE EFFECT ---
void fade_brightness(uint8_t from, uint8_t to, int duration_ms)
{
  const int steps = 25; // smoother
  float step_time = (float)duration_ms / steps;
  float delta = (to - from) / (float)steps;

  for (int i = 0; i <= steps; ++i)
  {
    uint8_t val = static_cast<uint8_t>(from + delta * i);
    DisplayManager::gfx.setBrightness(val);
    vTaskDelay(pdMS_TO_TICKS(step_time));
  }
}

// --- Display control helpers ---
void display_set_sleep(bool sleep)
{
  if (sleep)
  {
    ESP_LOGI(TAG, "Display sleeping...");
    fade_brightness(255, 0, 800); // fade out
  }
  else
  {
    ESP_LOGI(TAG, "Display waking...");
    fade_brightness(0, 255, 600); // fade in
  }
}

// --- Touchpad Read Callback ---
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  lv_coord_t x = 0;
  lv_coord_t y = 0;
  bool touched = DisplayManager::gfx.getTouch(&x, &y);

  // Adjust these to match your hardware / rotation
  // Try FLIP_Y = true to fix upside-down touches
  constexpr bool FLIP_X = false;
  constexpr bool FLIP_Y = true;
  constexpr bool SWAP_XY = false;

  if (touched)
  {
    lv_coord_t tx = x;
    lv_coord_t ty = y;

    if (SWAP_XY)
      std::swap(tx, ty);
    if (FLIP_X)
      tx = static_cast<lv_coord_t>(DisplayManager::gfx.screenWidth - tx - 1);
    if (FLIP_Y)
      ty = static_cast<lv_coord_t>(DisplayManager::gfx.screenHeight - ty - 1);

    data->point.x = tx;
    data->point.y = ty;
    data->state = LV_INDEV_STATE_PR;

    if (displaySleeping)
    {
      display_set_sleep(false);
      displaySleeping = false;
      data->state = LV_INDEV_STATE_REL;
      lastActivityTime = millis();
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}

// --- App setup ---
auto manualCalibration = display::ManualCalibration::instance();
auto firstScreen = display::FirstScreen::instance();

void setup()
{
  ESP_LOGI(TAG, "ES32 DCC Controller");

  display::calibrateState calibrated = manualCalibration->loadCalibrationFromNVS();

  ESP_LOGI(TAG, "Init Display");

  DisplayManager::gfx.begin();
  DisplayManager::gfx.setRotation(0);
  DisplayManager::gfx.fillScreen(TFT_BLACK);
  DisplayManager::gfx.setBrightness(255); // ensure display on at start

  if (calibrated == display::calibrateState::calibrate)
  {
    manualCalibration->calibrate();
    return;
  }

  lv_init();
  auto buffer_size = DisplayManager::bufferSize();
  buf1 = new lv_color_t[buffer_size];
  buf2 = new lv_color_t[buffer_size];
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buffer_size);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DisplayManager::gfx.screenWidth;
  disp_drv.ver_res = DisplayManager::gfx.screenHeight;
  disp_drv.flush_cb = DisplayManager::disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
  ESP_LOGI(TAG, "Touch indev registered: %p", indev);

  auto theme = std::make_shared<ui::LvglTheme>("Default");
  ui::LvglTheme::setActive(theme);

  switch (calibrated)
  {
  case display::calibrateState::calibrated:
    xTaskCreate([](void *parameters){
      vTaskDelay(pdMS_TO_TICKS(2000));
      utilities::WifiHandler::instance()->init_wifi();
      vTaskDelete(nullptr);
    }, "wifi_init_del", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
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

// Called by a periodic timer
void lv_tick_task(void *arg)
{
  (void)arg;
  lv_tick_inc(1); // exactly 1ms increment
}

extern "C" void app_main()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }
  else
  {
    ESP_ERROR_CHECK(ret);
  }

  setup();

  const esp_timer_create_args_t lv_tick_timer_args = {
      .callback = &lv_tick_task,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lv_tick",
      .skip_unhandled_events = false,
  };
  esp_timer_handle_t lv_tick_timer;
  ESP_ERROR_CHECK(esp_timer_create(&lv_tick_timer_args, &lv_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lv_tick_timer, 1000)); // 1000 µs = 1ms

  while (true)
  {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(10));

    // --- Inactivity check ---
    uint32_t now = millis();
    if (!displaySleeping && (now - lastActivityTime > INACTIVITY_TIMEOUT_MS))
    {
      display_set_sleep(true);
      displaySleeping = true;
    }
  }
}
