#pragma once
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>
#include "LvglWrapper.h"
namespace display {

class WifiListItem {
public:
  lv_obj_t *parentObj;
  size_t index;

  WifiListItem(lv_obj_t *parent, size_t index, std::string ssid, int8_t rssi): parentObj(parent), index(index), ssid(std::move(ssid)), rssi(rssi){
    lvObj = lv_list_add_btn(parent, LV_SYMBOL_WIFI, (this->ssid + " (" + std::to_string(this->rssi) + "dBm)").c_str());
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    setStylePart(lvObj, "wifi.item", LV_PART_MAIN);
    setStylePart(lvObj, "wifi.item.selected", LV_STATE_CHECKED);
  }

  // update RSSI and icon
  void setSignalStrength(int rssi) { this->rssi = rssi; }
  lv_obj_t* getLvObj() const { return lvObj; }
  std::string getSsid() const { return ssid; }
 
private:
  lv_obj_t *lvObj;
  std::string ssid;
  int8_t rssi = -100;
};
} // namespace display
