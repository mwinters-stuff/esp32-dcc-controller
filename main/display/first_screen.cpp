#include "esp_log.h"
#include <string>
#include "first_screen.h"
#include "calibrate_screen.h"

namespace display{

void FirstScreen::show(lv_obj_t * parent){

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr); /* clear existing children */

    // extern lv_font_t lv_font_montserrat_24;
    lbl_title = std::make_shared<ui::LvglLabel>(scr, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, &::lv_font_montserrat_30);
    lbl_title->setColor(lv_palette_main(LV_PALETTE_BLUE));

    /* Connect button */
     btn_connect = std::make_shared<ui::LvglButton>(
        scr,
        "Connect",
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Connect button clicked!");
            }
        },
        200, 48, LV_ALIGN_CENTER, 0, -60);

    /* Scan WiFi button */
    btn_wifi_scan = std::make_shared<ui::LvglButton>(
        scr,
        "Scan WiFi",
        [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Scan WiFi button clicked!");
            }
        },
        200, 48, LV_ALIGN_CENTER, 0, 0);

    /* Calibrate button */
    btn_cal = std::make_shared<ui::LvglButton>(
        scr,
        "Calibrate",
        [this,scr](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                LV_LOG_USER("Calibrate button clicked!");
                auto calScreen = std::make_shared<display::CalibrateScreen>(shared_from_this());
                calScreen->onExit = [this]() { this->show(); };
                calScreen->show(lv_scr_act());
            }
        },
        200, 48, LV_ALIGN_CENTER, 0, 60);

    ESP_LOGI(TAG, "Front Screen UI created");
}

void FirstScreen::cleanUp(){
    btn_cal.reset();
    btn_connect.reset();
    btn_wifi_scan.reset();
}

}