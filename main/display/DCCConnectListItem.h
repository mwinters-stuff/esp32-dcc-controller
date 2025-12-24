#pragma once
#include "ui/LvglStyle.h"
#include "ui/LvglTheme.h"
#include "utilities/WifiHandler.h"
#include "LvglWrapper.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class DCCConnectListItem {
public:
  static std::shared_ptr<DCCConnectListItem> currentListItem;

  DCCConnectListItem(lv_obj_t *parent, size_t index, utilities::WithrottleDevice device): device_(device){
    lvObj = lv_list_add_btn(parent, LV_SYMBOL_CALL,  (device.instance + " (" + device.ip + ":" + std::to_string(device.port) + ")").c_str());
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    setStylePart(lvObj, "wifi.item", LV_PART_MAIN);
    setStylePart(lvObj, "wifi.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t* getLvObj() const { return lvObj; }
      
  bool matches(const utilities::WithrottleDevice &other) const {
    return device_.instance == other.instance && device_.ip == other.ip && device_.port == other.port;
  }

  bool isSame(const utilities::WithrottleDevice &other) const {
    return device_.ip == other.ip && device_.port == other.port;
  }

  void update(const utilities::WithrottleDevice &other) {
    device_.ip = other.ip;
    device_.port = other.port;

    // Update displayed text
    std::string itemText = device_.instance + " (" + device_.ip + ":" + std::to_string(device_.port) + ")";
    lv_list_set_btn_text(lvObj, itemText.c_str());
  }

  std::string getText() const {
    return device_.instance + " (" + device_.ip + ":" + std::to_string(device_.port) + ")";
  }

  const utilities::WithrottleDevice &device() const { return device_; }

private:
  lv_obj_t *lvObj;
  utilities::WithrottleDevice device_;
};
} // namespace display
