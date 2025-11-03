#include <lvgl.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <nvs_handle.hpp>
#include <esp_system.h>
#include "display_manager.h"
#include "manual_calibration.h"
#include <array>
#include <cmath>

namespace display {

#define NVS_NAMESPACE "touch_cal"
#define NVS_CALIBRATION_SAVED "cal_saved"
#define NVS_CALIBRATION "calib"

void  ManualCalibration::show(lv_obj_t* parent){
   Screen::show(parent); // Ensure base setup (sets lvObj_, applies theme, etc.)

   lbl_title = std::make_shared<ui::LvglLabel>(
       lvObj_, "Calibrate Screen", LV_ALIGN_TOP_MID, 0, 20, &::lv_font_montserrat_30);
    lbl_title->setColor(lv_palette_main(LV_PALETTE_BLUE));

    lbl_sub_title = std::make_shared<ui::LvglLabel>(
        lvObj_, "Touch corners to calibrate", LV_ALIGN_CENTER, 0, 0);

    btn_start = std::make_shared<ui::LvglButton>(
        lvObj_, "Start Calibration",
        [this](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Start Calibration");

                rebootToCalibrate();
            }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -80
    );

}

struct CancelTimerData {
    std::shared_ptr<Screen> prev;
    ManualCalibration* self;
};

// file-local timer callback (use lv_timer_get_user_data to avoid accessing lv_timer_t fields)
static void cancel_timer_cb(lv_timer_t * t) {
    if (!t) return;
    void *ud = lv_timer_get_user_data(t);
    auto *d = static_cast<CancelTimerData*>(ud);
    if (d) {
        //d->self->cleanUp();

        if (d->prev) {
            d->prev->show();
        } else {
            ESP_LOGW("MANUAL_CALIBRATION", "Previous screen expired, nothing to show");
        }
        
        delete d;
    }
    lv_timer_del(t);
}

void ManualCalibration::addBackButton(std::weak_ptr<Screen> previousScreen){
    previousScreen_ = previousScreen;

    btn_cancel = std::make_shared<ui::LvglButton>(
        lvObj_, "Cancel",
        [this, previous = previousScreen_](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                ESP_LOGI(TAG, "Cancel Calibration");
                // lock now to obtain shared_ptr snapshot
                auto prev = previous.lock();

                // allocate data for timer
                auto *data = new CancelTimerData{ prev, this };

                // create a short-lived timer to run after the event handler returns
                lv_timer_t *tm = lv_timer_create(cancel_timer_cb, 1 /*ms*/, data);

                // fallback if timer creation failed
                if (!tm) {
                    if (prev) prev->show();
                    this->cleanUp();
                    delete data;
                }
            }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -20
    );
}

void ManualCalibration::cleanUp(){
    lbl_title.reset();
    lbl_sub_title.reset();
    btn_start.reset();
    btn_cancel.reset();
    // reset weak ref
    previousScreen_.reset();

    Screen::cleanUp();
}

void ManualCalibration::rebootToCalibrate(){
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READWRITE, &err);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    err = handle->set_item(NVS_CALIBRATION_SAVED, calibrateState::calibrate);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rebootToCalibrate calibration_saved: %s", esp_err_to_name(err));
        return;
    }

    err = handle->commit();
    if (err != ESP_OK) {
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
    switch(cal){
        case         calibrateState::notCalibrated:
            ESP_LOGI(TAG,"Calibrate State: notCalibrated");
            break;
        case         calibrateState::calibrate:
            ESP_LOGI(TAG,"Calibrate State: calibrate");
                break;
        case         calibrateState::showScreen:
            ESP_LOGI(TAG,"Calibrate State: showScreen");
                break;
        case         calibrateState::calibrated:
            ESP_LOGI(TAG,"Calibrate State: calibrated");
                break;
            default:
            ESP_LOGI(TAG,"Calibrate State: unknown");
                break;
    }
    return cal;
}

void ManualCalibration::save_to_nvs(calibrateState state){
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READWRITE, &err);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    err = handle->set_blob(NVS_CALIBRATION, parameters, sizeof(parameters));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Saving calibration failed: %s", esp_err_to_name(err));
    }    
    err = handle->set_item(NVS_CALIBRATION_SAVED, state);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Saving calibration_saved failed: %s", esp_err_to_name(err));
    }    
    err = handle->commit();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
        return;
    }


    ESP_LOGI(TAG, "Saved calibration to NVS");
}

calibrateState ManualCalibration::load_from_nvs(){
    esp_err_t err;
    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE, NVS_READONLY, &err);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing calibration data: %s", esp_err_to_name(err));
        return calibrateState::showScreen;
    }
   

    calibrateState saved;
    err = handle->get_item(NVS_CALIBRATION_SAVED, saved);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Loading calibration_saved failed: %s", esp_err_to_name(err));
        saved = calibrateState::notCalibrated;
    }
    if(saved != calibrateState::calibrated){
        return (calibrateState)saved;
    }

    err = handle->get_blob(NVS_CALIBRATION, parameters, sizeof(parameters));

    if(err != ESP_OK){
        ESP_LOGW(TAG, "Loading calibration failed: %s", esp_err_to_name(err));
        return calibrateState::showScreen;
    }
    
    return calibrateState::calibrated;
    
}

void ManualCalibration::calibrate(){
    DisplayManager::gfx.calibrateTouch(parameters, TFT_WHITE, TFT_BLACK, 15);
    save_to_nvs(calibrated);
    esp_restart();
}

} // namespace display
