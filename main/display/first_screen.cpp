#include "first_screen.h"
#include "WifiListScreen.h"
#include "manual_calibration.h"
#include <esp_log.h>

namespace display
{

    void FirstScreen::show(lv_obj_t *parent)
    {
        Screen::show(parent); // Ensure base setup (sets lvObj_, applies theme, etc.)

        // Title Label
        lbl_title = std::make_shared<ui::LvglLabel>(
            lvObj_, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, &lv_font_montserrat_30);
        lbl_title->setStyle("label.title");
        lbl_title->setColor(lv_palette_main(LV_PALETTE_BLUE));

        // "Connect" button
        btn_connect = std::make_shared<ui::LvglButton>(
            lvObj_, "Connect",
            [this](lv_event_t *e)
            {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED)
                    ESP_LOGI(TAG, "Connect button clicked!");
            },
            200, 48, LV_ALIGN_CENTER, 0, -60);
        btn_connect->setStyle("button.primary");

        // "Scan WiFi" button
        btn_wifi_scan = std::make_shared<ui::LvglButton>(
            lvObj_, "Scan WiFi",
            [this](lv_event_t *e)
            {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED)
                {
                    ESP_LOGI(TAG, "Scan WiFi button clicked!");
                    auto wifiScreen = WifiListScreen::instance();
                    wifiScreen->show();
                    wifiScreen->setWifiList({
                        {"Network_1", -40},
                        {"Network_2", -70},
                        {"Network_3", -80},
                        {"Network_4", -90},
                    });
                }
            },
            200, 48, LV_ALIGN_CENTER, 0, 0);
        btn_wifi_scan->setStyle("button.primary");

        // "Calibrate" button
        btn_cal = std::make_shared<ui::LvglButton>(
            lvObj_, "Calibrate",
            [this](lv_event_t *e)
            {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED)
                {
                    ESP_LOGI(TAG, "Calibrate button clicked!");
                    auto calScreen = ManualCalibration::instance();
                    calScreen->show();
                    calScreen->addBackButton(FirstScreen::instance());
                }
            },
            200, 48, LV_ALIGN_CENTER, 0, 60);
        
        btn_cal->setStyle("button.secondary");

        ESP_LOGI(TAG, "FirstScreen UI created");
    }

    void FirstScreen::cleanUp()
    {
        lbl_title.reset();
        btn_connect.reset();
        btn_wifi_scan.reset();
        btn_cal.reset();
        Screen::cleanUp();
    }

} // namespace display
