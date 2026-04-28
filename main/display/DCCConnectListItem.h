#pragma once
#include "LvglWrapper.h"
#include "ui/LvglStyle.h"
#include "ui/LvglTheme.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class DCCConnectListItem {
public:
  static std::shared_ptr<DCCConnectListItem> currentListItem;

  DCCConnectListItem(lv_obj_t *parent, size_t index, utilities::WithrottleDevice device, bool isSaved = false)
      : device_(device) {
    const char *icon = isSaved ? LV_SYMBOL_SAVE : LV_SYMBOL_WIFI;
    lvObj = lv_list_add_btn_mode(parent, icon,
                                 (device.instance + " (" + device.ip + ":" + std::to_string(device.port) + ")").c_str(),
                                 LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    setStylePart(lvObj, "wifi.item", LV_PART_MAIN);
    setStylePart(lvObj, "wifi.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t *getLvObj() const { return lvObj; }

  bool matches(const utilities::WithrottleDevice &other) const {
    // Endpoint identity is IP+port; instance/hostname can legitimately differ
    // between saved and mDNS-discovered records.
    return device_.ip == other.ip && device_.port == other.port;
  }

  bool isSame(const utilities::WithrottleDevice &other) const {
    return device_.instance == other.instance && device_.hostname == other.hostname && device_.ip == other.ip &&
           device_.port == other.port;
  }

  void update(const utilities::WithrottleDevice &other) {
    device_.instance = other.instance;
    device_.hostname = other.hostname;
    device_.ip = other.ip;
    device_.port = other.port;
    device_.txt = other.txt;

    // Update displayed text
    std::string itemText = getText();
    lv_list_set_btn_text(lvObj, itemText.c_str());
  }

  std::string getText() const {
    const std::string &name = !device_.instance.empty()   ? device_.instance
                              : !device_.hostname.empty() ? device_.hostname
                                                          : device_.ip;
    return name + " (" + device_.ip + ":" + std::to_string(device_.port) + ")";
  }

  const utilities::WithrottleDevice &device() const { return device_; }

private:
  lv_obj_t *lvObj;
  utilities::WithrottleDevice device_;
};
} // namespace display
