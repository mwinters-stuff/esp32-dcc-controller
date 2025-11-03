#include "WifiListScreen.h"
#include <esp_log.h>

namespace display {
void WifiListScreen::show(lv_obj_t* parent) {
    Screen::show(parent);  // Ensure base setup (sets lvObj_, applies theme, etc.)
    
      // Full-screen container
    lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLLABLE);

    // Title label at top
    lbl_title_ = std::make_unique<LvglLabel>(lvObj_, "WiFi List", LV_ALIGN_TOP_MID, 0, 8);
    // Make title stand out if theme provides label.large
    if (auto style = LvglTheme::active() ? LvglTheme::active()->get("label.main") : nullptr) {
        style->applyTo(lbl_title_->lvObj(), LV_PART_MAIN);
    }

    // List view below
    list_view_ = std::make_unique<LvglListView>(lvObj_);
    lv_obj_set_height(list_view_->lvObj(), LV_PCT(86));
    lv_obj_align(list_view_->lvObj(), LV_ALIGN_BOTTOM_MID, 0, 0);
    
    ESP_LOGI("WIFI_LIST_SCREEN", "WifiListScreen shown");

}

void WifiListScreen::cleanUp() {
    ESP_LOGI("WIFI_LIST_SCREEN", "WifiListScreen cleaned up");
    Screen::cleanUp();
    lbl_title_.reset();
    list_view_.reset();
    items_.clear();
}


void WifiListScreen::setWifiList(const std::vector<std::pair<std::string,int>>& wifiList,
                                 WifiListItem::TapCallback globalTap) {
    // remove previous items
    for (auto &it : items_) {
        if (it && it->lvObj()) lv_obj_del(it->lvObj());
    }
    items_.clear();

    // add new items
    for (const auto& [ssid, rssi] : wifiList) {
        auto item = std::make_unique<WifiListItem>(list_view_->lvObj(), ssid, rssi, globalTap);
        items_.push_back(std::move(item));
    }

    ESP_LOGI("WIFI_LIST_SCREEN", "Wifi list updated with %zu items", items_.size());
}

};
