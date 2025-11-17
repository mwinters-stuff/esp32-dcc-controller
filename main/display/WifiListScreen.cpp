#include "WifiListScreen.h"
#include "WifiConnectScreen.h"
#include "definitions.h"
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <regex.h>
#include <string>

namespace display
{
  std::shared_ptr<WifiListItem> WifiListItem::currentWifiItem = nullptr;
  lv_obj_t *WifiListItem::currentButton;

  static const char *TAG = "WIFI_LIST_SCREEN";

  void WifiListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen)
  {
    Screen::show(parent, parentScreen); // Ensure base setup (sets lvObj_, applies theme, etc.)

    // Full-screen container
    lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLLABLE);

    // Title label at top
    lbl_title_ = std::make_unique<LvglLabel>(lvObj_, "WiFi List", LV_ALIGN_TOP_MID, 0, 8);
    lbl_title_->setStyle("label.title");

    btn_back_ = std::make_unique<LvglButton>(lvObj_, "Back", [this](lv_event_t *e)
                                             {
                                                     // Handle back action
                                                     if (lv_event_get_code(e) == LV_EVENT_CLICKED)
                                                     {
                                                         ESP_LOGI("WIFI_LIST_SCREEN", "Back button pressed");
                                                         if (auto screen = parentScreen_.lock())
                                                         {
                                                             screen->show();
                                                         }
                                                     } });
    btn_back_->setStyle("button.secondary");
    btn_back_->setAlignment(LV_ALIGN_BOTTOM_LEFT, 8, -12);
    btn_back_->setSize(100, 40);

    btn_connect_ = std::make_unique<LvglButton>(lvObj_, "Connect", [](lv_event_t *e)
                                                {
                                                  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
                                                  {
                                                    ESP_LOGI("WIFI_LIST_SCREEN", "Connect button pressed");
                                                    if (WifiConnectScreen::instance() && WifiListItem::currentWifiItem)
                                                    {

                                                      WifiConnectScreen::instance()->setSSID(WifiListItem::currentWifiItem->ssid());
                                                      WifiConnectScreen::instance()->show(nullptr, WifiListScreen::instance());
                                                    }
                                                  }
                                                  // Handle connect action
                                                });
    btn_connect_->setStyle("button.primary");
    btn_connect_->setAlignment(LV_ALIGN_BOTTOM_RIGHT, -8, -12);
    btn_connect_->setSize(100, 40);

    // List view below
    list_view_ = std::make_unique<LvglListView>(lvObj_, 0, 40, 320, 380);

    ESP_LOGI("WIFI_LIST_SCREEN", "WifiListScreen shown");

    items_.clear();

    // Create spinner overlay (centered)
    spinner_ = std::make_unique<LvglSpinner>(lvObj_);

    // Start Wi-Fi scan in a separate task
    xTaskCreatePinnedToCore(
        [](void *param)
        {
          auto *self = static_cast<WifiListScreen *>(param);
          self->scanWifiTask();
          vTaskDelete(nullptr);
        },
        "wifi_scan_task",
        4096, // stack size
        this,
        5, // priority
        nullptr,
        1 // run on core 1 (UI often on core 0)
    );
  }

  void WifiListScreen::cleanUp()
  {
    ESP_LOGI("WIFI_LIST_SCREEN", "WifiListScreen cleaned up");
    Screen::cleanUp();
    lbl_title_.reset();
    list_view_.reset();
    items_.clear();
    btn_back_.reset();
    btn_connect_.reset();
    spinner_.reset();
  }

  void WifiListScreen::scanWifiTask()
  {
    ESP_LOGI(TAG, "Starting background Wi-Fi scan...");
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_scan_config_t scan_conf = {};
    scan_conf.ssid = nullptr;
    scan_conf.bssid = nullptr;
    scan_conf.channel = 0;
    scan_conf.show_hidden = true;

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_conf, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count == 0)
    {
      ESP_LOGI(TAG, "No APs found");
      lv_async_call([](void *data)
                    {
            auto self = static_cast<WifiListScreen *>(data);
            self->spinner_->hideSpinner(); }, this);
      return;
    }

    uint16_t max_records = std::min<uint16_t>(ap_count, DEFAULT_SCAN_LIST_SIZE);
    std::vector<wifi_ap_record_t> ap_info(max_records);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_records, ap_info.data()));

    // Copy results into heap before passing to LVGL thread
    auto *results = new std::vector<wifi_ap_record_t>(ap_info);

    // Now update UI safely
    lv_async_call([](void *data)
                  {
        auto *ctx = static_cast<std::pair<WifiListScreen *, std::vector<wifi_ap_record_t> *> *>(data);
        auto *self = ctx->first;
        auto *records = ctx->second;
        self->populateList(*records);
        self->spinner_->hideSpinner();
        delete records;
        delete ctx; 
      }, new std::pair<WifiListScreen *, std::vector<wifi_ap_record_t> *>(this, results));

    ESP_ERROR_CHECK(esp_wifi_scan_stop());
  }

  void WifiListScreen::populateList(const std::vector<wifi_ap_record_t> &records)
  {
    for (const auto &ap : records)
    {
      std::string ssid_str(reinterpret_cast<const char *>(ap.ssid),
                           strnlen(reinterpret_cast<const char *>(ap.ssid), sizeof(ap.ssid)));
      if (ssid_str.empty())
        ssid_str = "<hidden>";

      auto item = std::make_shared<WifiListItem>(list_view_->lvObj(), ssid_str, ap.rssi);
      items_.push_back(item);
    }

    ESP_LOGI(TAG, "Wi-Fi scan complete, list populated with %u entries", (unsigned)records.size());
  }

};
