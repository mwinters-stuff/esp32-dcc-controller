#include "ManualCalibration.h"
#include "DisplayManager.h"
#include "definitions.h"
#include <array>
#include <cmath>
#include <esp_log.h>
#include <esp_system.h>
#include <lvgl.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <nvs_handle.hpp>

namespace display
{

  static const char *TAG = "MANUAL_CALIBRATION";

  void ManualCalibration::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen)
  {
    Screen::show(parent, parentScreen); // Ensure base setup (sets lvObj_, applies theme, etc.)

    lbl_title = std::make_shared<ui::LvglLabel>(
        lvObj_, "Calibrate Screen", LV_ALIGN_TOP_MID, 0, 20);
    lbl_title->setStyle("label.title");

    lbl_sub_title = std::make_shared<ui::LvglLabel>(
        lvObj_, "Touch corners to calibrate", LV_ALIGN_CENTER, 0, 0);
    lbl_sub_title->setStyle("label.main");

    btn_start = std::make_shared<ui::LvglButton>(
        lvObj_, "Start Calibration",
        [this](lv_event_t *e)
        {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED)
          {
            LV_LOG_USER("Start Calibration");

            rebootToCalibrate();
          }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -80);
    btn_start->setStyle("button.primary");

    if (parentScreen_.expired() == false)
    {
      btn_back = std::make_shared<ui::LvglButton>(
          lvObj_, "Back",
          [this](lv_event_t *e)
          {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED)
            {
              LV_LOG_USER("Back button clicked");
              if (auto screen = parentScreen_.lock())
              {
                screen->show();
              }
            }
          },
          200, 48, LV_ALIGN_BOTTOM_MID, 0, -20);
      btn_back->setStyle("button.secondary");
    }
  }

  void ManualCalibration::cleanUp()
  {
    lbl_title.reset();
    lbl_sub_title.reset();
    btn_start.reset();
    btn_back.reset();
  }

  void ManualCalibration::rebootToCalibrate()
  {
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READWRITE, &err);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
      return;
    }

    err = handle->set_item(NVS_CALIBRATION_SAVED, calibrateState::calibrate);
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "rebootToCalibrate calibration_saved: %s", esp_err_to_name(err));
      return;
    }

    err = handle->commit();
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
      return;
    }

    ESP_LOGI(TAG, "Saved calibration to NVS Rebooting");
    sleep(1);
    esp_restart();
  }

  calibrateState ManualCalibration::loadCalibrationFromNVS()
  {
    auto cal = load_from_nvs();
    switch (cal)
    {
    case calibrateState::notCalibrated:
      ESP_LOGI(TAG, "Calibrate State: notCalibrated");
      break;
    case calibrateState::calibrate:
      ESP_LOGI(TAG, "Calibrate State: calibrate");
      break;
    case calibrateState::showScreen:
      ESP_LOGI(TAG, "Calibrate State: showScreen");
      break;
    case calibrateState::calibrated:
      ESP_LOGI(TAG, "Calibrate State: calibrated");
      break;
    default:
      ESP_LOGI(TAG, "Calibrate State: unknown");
      break;
    }
    return cal;
  }

  void ManualCalibration::save_to_nvs(calibrateState state)
  {
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READWRITE, &err);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
      return;
    }

    err = handle->set_blob(NVS_CALIBRATION, parameters, sizeof(parameters));
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "Saving calibration failed: %s", esp_err_to_name(err));
    }
    err = handle->set_item(NVS_CALIBRATION_SAVED, state);
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "Saving calibration_saved failed: %s", esp_err_to_name(err));
    }
    err = handle->commit();
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
      return;
    }

    ESP_LOGI(TAG, "Saved calibration to NVS");
  }

  calibrateState ManualCalibration::load_from_nvs()
  {
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READONLY, &err);
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "No existing calibration data: %s", esp_err_to_name(err));
      return calibrateState::showScreen;
    }

    calibrateState saved;
    err = handle->get_item(NVS_CALIBRATION_SAVED, saved);
    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "Loading calibration_saved failed: %s", esp_err_to_name(err));
      saved = calibrateState::notCalibrated;
    }
    if (saved != calibrateState::calibrated)
    {
      return (calibrateState)saved;
    }

    err = handle->get_blob(NVS_CALIBRATION, parameters, sizeof(parameters));

    if (err != ESP_OK)
    {
      ESP_LOGW(TAG, "Loading calibration failed: %s", esp_err_to_name(err));
      return calibrateState::showScreen;
    }

    return calibrateState::calibrated;
  }

  void ManualCalibration::calibrate()
  {
    DisplayManager::gfx.calibrateTouch(parameters, TFT_WHITE, TFT_BLACK, 15);
    save_to_nvs(calibrated);
    esp_restart();
  }

} // namespace display
