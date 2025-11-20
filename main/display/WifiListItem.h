#pragma once
#include "ui/LvglListItem.h"
#include "ui/LvglStyle.h"
#include "ui/LvglTheme.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class WifiListItem : public ui::LvglListItem {
public:
  static std::shared_ptr<WifiListItem> currentWifiItem;

  WifiListItem(lv_obj_t *parent, size_t index, std::string ssid, int8_t rssi)
      : ui::LvglListItem(parent, index, LV_SYMBOL_WIFI, (ssid + " (" + std::to_string(rssi) + "dBm)").c_str()),
        ssid_(std::move(ssid)), rssi_(rssi) {}

  // update RSSI and icon
  void setSignalStrength(int rssi) { rssi_ = rssi; }

  const std::string &ssid() const { return ssid_; }

private:
  std::string ssid_;
  int8_t rssi_ = -100;
};
} // namespace display
