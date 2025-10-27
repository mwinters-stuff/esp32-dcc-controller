#include "esp_log.h"
#include <string>
#include <atomic>
#include "calibrate_screen.h"
#include "display_manager.h"

namespace display{

    static lv_obj_t* s_lbl_sub_title = nullptr;

void CalibrateScreen::show(lv_obj_t* parent) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_parent(scr, parent);
    lv_obj_clean(scr);

    lbl_title = std::make_shared<ui::LvglLabel>(
        scr, "Calibrate Screen", LV_ALIGN_TOP_MID, 0, 20, &::lv_font_montserrat_30);
    lbl_title->setColor(lv_palette_main(LV_PALETTE_BLUE));

    lbl_sub_title = std::make_shared<ui::LvglLabel>(
        scr, "Touch corners to calibrate", LV_ALIGN_CENTER, 0, 0);

    btn_save = std::make_shared<ui::LvglButton>(
        scr, "Save Calibration",
        [this](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Save Calibration");
                if (onExit) onExit();
            }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -80
    );

    btn_cancel = std::make_shared<ui::LvglButton>(
        scr, "Cancel",
        [this](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Cancel Button Clicked");
                if (onExit) onExit();
            }
        },
        200, 48, LV_ALIGN_BOTTOM_MID, 0, -20
    );

    btn_save->setEnabled(false);
    btn_cancel->setEnabled(false);

    ESP_LOGI(TAG, "Calibrate screen UI shown");
    lv_timer_handler(); // ensure LVGL has rendered labels/buttons

    // Run calibration (LVGL will redraw after)
    runTouchCalibration();

    btn_save->setEnabled(true);
    btn_cancel->setEnabled(true);


}

static void lv_update_label_cb(void* param) {
    const char* txt = static_cast<const char*>(param);
    if (s_lbl_sub_title && lv_obj_is_valid(s_lbl_sub_title)) {
        lv_label_set_text(s_lbl_sub_title, txt);
    }
}

// put this near top of your .cpp
static void __attribute__((iram_attr)) calibration_task(void* arg) {
    // best-effort end any active write
    if (DisplayManager::gfx.getStartCount() > 0) {
        DisplayManager::gfx.endWrite();
    }

    // tell LVGL to use blocking flush
    DisplayManager::calibrating.store(true, std::memory_order_release);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI("CalTask", "Calling calibrateTouch() from task %s", pcTaskGetName(NULL));

    // NOTE: calibrateTouch() itself lives in flash (library). If it disables cache,
    // code that executes from flash inside it may still fault. Trying IRAM task often helps.
    DisplayManager::gfx.calibrateTouch(nullptr, TFT_WHITE, TFT_BLACK, 15);

    ESP_LOGI("CalTask", "calibrateTouch() finished");

    DisplayManager::calibrating.store(false, std::memory_order_release);

    const char* done_text = "Calibration complete!";
    lv_async_call(lv_update_label_cb, (void*)done_text);

    vTaskDelete(NULL);
}



void CalibrateScreen::runTouchCalibration() {
    ESP_LOGI(TAG, "Starting touchscreen calibration...");

    if (lbl_sub_title) s_lbl_sub_title = lbl_sub_title->getLvObj();

    // ensure LVGL has drawn the "Touch corners..." prompt before we start
    lv_timer_handler();
    lv_refr_now(lv_disp_get_default());

    // Create a FreeRTOS task for calibration (adjust stack size as needed)
    BaseType_t ok = xTaskCreatePinnedToCore(
        calibration_task,
        "CalTask",
        8192,          // stack size in words — bump if you see stack overflow
        nullptr,
        tskIDLE_PRIORITY + 5,
        nullptr,
        1 // or tskNO_AFFINITY or the core you want
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create calibration task");
        // fallback: run in-place (not ideal) or update UI about failure
    }
}

void CalibrateScreen::cleanUp() {
    lbl_title.reset();
    lbl_sub_title.reset();
    btn_cancel.reset();
    btn_save.reset();
}

}