#include <lvgl.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_system.h>
#include "display_manager.h"
#include "manual_calibration.h"
#include <array>
#include <cmath>

namespace display {

#define NVS_NAMESPACE "touch_cal"
#define NVS_CALIBRATION_SAVED "cal_saved"
#define NVS_CALIBRATION "calibration"

void  ManualCalibration::show(lv_obj_t* parent){
    lv_obj_t* scr = lv_scr_act();
    
    lv_obj_clean(scr);

    lbl_title = std::make_shared<ui::LvglLabel>(
        scr, "Calibrate Screen", LV_ALIGN_TOP_MID, 0, 20, &::lv_font_montserrat_30);
    lbl_title->setColor(lv_palette_main(LV_PALETTE_BLUE));

    lbl_sub_title = std::make_shared<ui::LvglLabel>(
        scr, "Touch corners to calibrate", LV_ALIGN_CENTER, 0, 0);

    btn_start = std::make_shared<ui::LvglButton>(
        scr, "Start Calibration",
        [this](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Start Calibration");

                rebootToCalibrate();
            }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -80
    );


}

void ManualCalibration::cleanUp(){
    lbl_title.reset();
    lbl_sub_title.reset();
    btn_start.reset();
}

void ManualCalibration::rebootToCalibrate(){
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i8(nvs,NVS_CALIBRATION_SAVED, calibrateState::calibrate);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGW(TAG, "rebootToCalibrate calibration_saved");
        return;
    }

    nvs_commit(nvs);
    nvs_close(nvs);

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

void ManualCalibration::save_to_nvs(calibrateState state)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(nvs, NVS_CALIBRATION, parameters, sizeof(parameters));
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGW(TAG, "Saving calibration failed");
    }    
    err = nvs_set_i8(nvs,NVS_CALIBRATION_SAVED, state);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGW(TAG, "Saving calibration_saved failed");
    }    
    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Saved calibration to NVS");
}

calibrateState ManualCalibration::load_from_nvs()
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGW(TAG, "No existing calibration data");
        return calibrateState::showScreen;
    }
    

    int8_t saved;
    err = nvs_get_i8(nvs, NVS_CALIBRATION_SAVED, &saved);
    if (err != ESP_OK) {
        showError(err);
        ESP_LOGW(TAG, "Loading calibration_saved failed");
        saved = calibrateState::notCalibrated;
    }
    if(saved != calibrateState::calibrated){
        nvs_close(nvs);
        return (calibrateState)saved;
    }

    size_t length;
    err = nvs_get_blob(nvs, NVS_CALIBRATION, parameters, &length);

    if(err != ESP_OK || length != sizeof(parameters)){
        showError(err);
        ESP_LOGW(TAG, "Loading calibration failed");
        nvs_close(nvs);
        return calibrateState::showScreen;
    }
    
    nvs_close(nvs);
    return calibrateState::calibrated;
    
}

void ManualCalibration::calibrate(){
    DisplayManager::gfx.calibrateTouch(parameters, TFT_WHITE, TFT_BLACK, 15);
    save_to_nvs(calibrated);
    esp_restart();
}

} // namespace display
