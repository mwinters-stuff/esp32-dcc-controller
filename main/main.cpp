#include "display/display_manager.h"
#include "display/first_screen.h"
#include "display/manual_calibration.h"
#include "millis.h"
#include <LGFX_ILI9488_S3.hpp>
#include <LovyanGFX.hpp>
#include <atomic>
#include <chrono>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <lvgl.h>
#include <memory>
#include <nvs_flash.h>
#include <string>

namespace main
{
  const char *TAG = "main";

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
    int16_t x, y;
    bool touched = display::read_raw_touch(&x, &y);

    if (touched)
    {
      display::applyTouchTransform(&x, &y);
      if (displaySleeping)
      {
        display_set_sleep(false);
        displaySleeping = false;
        touched = false;
      }
    }

    data->point.x = x;
    data->point.y = y;
    data->state = touched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  }

  // --- App setup ---
  std::shared_ptr<display::FirstScreen> firstScreen;

  void setup()
  {
    ESP_LOGI(TAG, "ES32 DCC Controller");

    if (display::loadCalibrationFromNVS())
    {
      ESP_LOGI("APP", "Touchscreen already calibrated");
    }
    else
    {
      // run calibration UI once
      display::startManualCalibration(lv_scr_act());
    }

    ESP_LOGI(TAG, "Init Display");

    DisplayManager::gfx.begin();
    DisplayManager::gfx.setRotation(0);
    DisplayManager::gfx.fillScreen(TFT_BLACK);
    DisplayManager::gfx.setBrightness(255); // ensure display on at start

    lv_init();
    auto buffer_size = DisplayManager::bufferSize();
    buf1 = new lv_color_t[buffer_size];
    buf2 = new lv_color_t[buffer_size];
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2,buffer_size );

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

    firstScreen = std::make_shared<display::FirstScreen>();
    firstScreen->show(NULL);

    lastActivityTime = millis();
    ESP_LOGI(TAG, "Setup complete. UI should be visible.");
  }

} // namespace main

extern "C" void app_main()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  main::setup();

  while (true)
  {
    lv_tick_inc(2);
    lv_timer_handler();

    // --- Inactivity check ---
    uint32_t now = millis();
    if (!main::displaySleeping && (now - main::lastActivityTime > main::INACTIVITY_TIMEOUT_MS))
    {
      main::display_set_sleep(true);
      main::displaySleeping = true;
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
