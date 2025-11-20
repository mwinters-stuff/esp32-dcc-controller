#pragma once
#include "ui/LvglListItem.h"
#include "ui/LvglListView.h"
#include "ui/LvglStyle.h"
#include "ui/LvglTheme.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class DCCConnectListItem : public ui::LvglListItem {
public:
  static std::shared_ptr<DCCConnectListItem> currentListItem;

  DCCConnectListItem(lv_obj_t *parent, size_t index, utilities::WithrottleDevice device)
      : LvglListItem(parent, index, LV_SYMBOL_CALL,
                     device.instance + " (" + device.ip + ":" + std::to_string(device.port) + ")"),
        device_(device) {}

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
    ui::LvglListView::lv_list_set_btn_text(lvObj(), itemText.c_str());
  }

  const utilities::WithrottleDevice &device() const { return device_; }

private:
  utilities::WithrottleDevice device_;
};
} // namespace display
