#pragma once
#include "ui/LvglStyle.h"
#include "ui/LvglTheme.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display
{

  class WifiListItem : public std::enable_shared_from_this<WifiListItem>
  {
  public:
    static std::shared_ptr<WifiListItem> currentWifiItem;

    lv_obj_t *lvObj() const { return lvObj_; }

    WifiListItem(lv_obj_t *parent,
                 std::string ssid,
                 int8_t rssi)
        : parentObj_(parent), ssid_(std::move(ssid)), rssi_(rssi)
    {
      lvObj_ = lv_list_add_btn(parent, LV_SYMBOL_WIFI, (ssid_ + " (" + std::to_string(rssi) + "dBm)").c_str());

      // Register click event
      lv_obj_add_event_cb(lvObj_, &WifiListItem::event_trampoline, LV_EVENT_CLICKED, this);

      // Apply theme styles now
      applyTheme();
    }

    // update RSSI and icon
    void setSignalStrength(int rssi)
    {
      rssi_ = rssi;
    }

    const std::string &ssid() const { return ssid_; }

    // theme application (LVGL 8.4)
    void applyTheme()
    {
      auto theme = ui::LvglTheme::active();
      if (!theme)
        return;

      const ui::LvglStyle *style = theme->get("wifi.item");
      if (style)
        style->applyTo(lvObj_, LV_PART_MAIN);

      static lv_style_t style_checked;
      lv_style_init(&style_checked);
      lv_style_set_bg_color(&style_checked, lv_color_hex(0x3399FF)); // Blue highlight
      lv_style_set_bg_opa(&style_checked, LV_OPA_50);
      lv_style_set_border_color(&style_checked, lv_color_hex(0x0066CC));
      lv_style_set_border_width(&style_checked, 2);

      lv_obj_add_style(lvObj_, &style_checked, LV_STATE_CHECKED);
    }

  private:
    lv_obj_t *parentObj_ = nullptr;
    lv_obj_t *lvObj_ = nullptr;

    static lv_obj_t *currentButton;
    static void event_trampoline(lv_event_t *e)
    {
      auto btn = static_cast<WifiListItem *>(lv_event_get_user_data(e))->shared_from_this();

      lv_event_code_t code = lv_event_get_code(e);

      if (code == LV_EVENT_CLICKED)
      {
        ESP_LOGI("WifiListItem", "Clicked: %s", lv_list_get_btn_text(btn->parentObj_, btn->lvObj_));

        if (currentButton == btn->lvObj_)
        {
          currentButton = NULL;
          currentWifiItem = nullptr;
        }
        else
        {
          currentButton = btn->lvObj_;
          currentWifiItem = btn;
        }

        uint32_t i;
        for (i = 0; i < lv_obj_get_child_cnt(btn->parentObj_); i++)
        {
          lv_obj_t *child = lv_obj_get_child(btn->parentObj_, i);
          if (child == currentButton)
          {
            lv_obj_add_state(child, LV_STATE_CHECKED);
          }
          else
          {
            lv_obj_clear_state(child, LV_STATE_CHECKED);
          }
        }
      }
    }

    std::string ssid_;
    int8_t rssi_ = -100;
  };
} // namespace display
