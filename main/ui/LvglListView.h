#pragma once
#include <memory>
#include "LvglWidgetBase.h"
#include <lvgl.h>
#include "LvglTheme.h"
#include "LvglStyle.h"
#include "WifiListItem.h"

namespace ui {



class LvglListView : public LvglWidgetBase {
public:
    LvglListView(lv_obj_t* parent)
        : LvglWidgetBase(lv_list_create(parent))
    {
        // full width, allow scrolling vertically
        lv_obj_set_size(lvObj_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_scroll_dir(lvObj_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(lvObj_, LV_SCROLLBAR_MODE_AUTO);
        applyTheme();
    }

    void applyTheme() override {
        auto theme = LvglTheme::active();
        if (!theme) return;
        const LvglStyle* style = theme->get("list.main");
        lv_obj_remove_style_all(lvObj_);
        if (style) style->applyTo(lvObj_, LV_PART_MAIN);
    }

    // convenience for external code to add an item (returns pointer to created item)
    std::shared_ptr<WifiListItem> addWifiItem(const std::string& ssid, int rssi,
                                              WifiListItem::TapCallback onTap = nullptr,
                                              WifiListItem::RawEventCallback rawCb = nullptr)
    {
        // leave construction to caller's header for WifiListItem; here create and wrap
        auto item = std::make_shared<WifiListItem>(lvObj_, ssid, rssi, std::move(onTap), std::move(rawCb));
        return item;
    }
};

} // namespace ui
