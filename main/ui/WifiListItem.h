#pragma once
#include <string>
#include <functional>
#include <memory>
#include <lvgl.h>
#include "LvglWidgetBase.h"
#include "LvglLabel.h"
#include "LvglTheme.h"
#include "LvglStyle.h"

namespace ui {

class WifiListItem : public LvglWidgetBase {
public:
    using TapCallback = std::function<void(const std::string& ssid)>;
    using RawEventCallback = std::function<void(lv_event_t* e)>;

    WifiListItem(lv_obj_t* parent,
                 std::string ssid,
                 int rssi,
                 TapCallback onTap = nullptr,
                 RawEventCallback rawCb = nullptr)
        : LvglWidgetBase(nullptr), ssid_(std::move(ssid)), onTap_(std::move(onTap)), rawCb_(std::move(rawCb))
    {
        // Create a list button (icon nullptr, text empty because we'll use labels)
        lvObj_ = lv_list_add_btn(parent, nullptr, "");
        lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

        // Create SSID label (left)
        lbl_ssid_ = std::make_unique<LvglLabel>(lvObj_, ssid_, LV_ALIGN_LEFT_MID, 8, 0);

        // Create signal label (right) and set icon text
        lbl_signal_ = std::make_unique<LvglLabel>(lvObj_, getSignalIcon(rssi), LV_ALIGN_RIGHT_MID, -8, 0);

        // keep an internal RSSI value
        rssi_ = rssi;

        // Register click event
        lv_obj_add_event_cb(lvObj_, &WifiListItem::event_trampoline, LV_EVENT_CLICKED, this);

        // Apply theme styles now
        applyTheme();
    }

    // high-level tap callback
    void setOnTap(TapCallback cb) { onTap_ = std::move(cb); }

    // low-level raw LVGL event callback
    void setRawEventCb(RawEventCallback cb) { rawCb_ = std::move(cb); }

    // update RSSI and icon
    void setSignalStrength(int rssi) {
        rssi_ = rssi;
        lbl_signal_->setText(getSignalIcon(rssi).c_str());
    }

    const std::string& ssid() const { return ssid_; }

    // theme application (LVGL 8.4)
    void applyTheme() override {
        auto theme = LvglTheme::active();
        if (!theme) return;

        const LvglStyle* style = theme->get("wifi.item");
        lv_obj_remove_style_all(lvObj_);
        if (style) style->applyTo(lvObj_, LV_PART_MAIN);

        // Also apply label style if available
        const LvglStyle* labelStyle = theme->get("label.main");
        if (labelStyle) {
            labelStyle->applyTo(lbl_ssid_->lvObj(), LV_PART_MAIN);
            labelStyle->applyTo(lbl_signal_->lvObj(), LV_PART_MAIN);
        }
    }

private:
    static void event_trampoline(lv_event_t* e) {
        auto *self = static_cast<WifiListItem*>(lv_event_get_user_data(e));
        if (!self) return;

        // Call raw callback if present
        if (self->rawCb_) self->rawCb_(e);

        // Call high-level tap callback with SSID
        if (self->onTap_) self->onTap_(self->ssid_);
    }

    // Convert RSSI to a little textual icon using LVGL symbol (no images required)
    static std::string getSignalIcon(int rssi) {
        int level = 0;
        if (rssi > -50) level = 4;
        else if (rssi > -60) level = 3;
        else if (rssi > -70) level = 2;
        else if (rssi > -80) level = 1;
        else level = 0;

        switch (level) {
            case 4: return std::string(LV_SYMBOL_WIFI) + "▂▃▄▅";
            case 3: return std::string(LV_SYMBOL_WIFI) + "▂▃▄";
            case 2: return std::string(LV_SYMBOL_WIFI) + "▂▃";
            case 1: return std::string(LV_SYMBOL_WIFI) + "▂";
            default: return std::string(LV_SYMBOL_CLOSE);
        }
    }

    std::string ssid_;
    int rssi_ = -100;

    std::unique_ptr<LvglLabel> lbl_ssid_;
    std::unique_ptr<LvglLabel> lbl_signal_;

    TapCallback onTap_;
    RawEventCallback rawCb_;
};

} // namespace ui
