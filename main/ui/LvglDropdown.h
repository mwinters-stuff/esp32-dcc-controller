#ifndef _LVGL_DROPDOWN_H
#define _LVGL_DROPDOWN_H

#include <lvgl.h>
#include <functional>
#include <string>
#include <vector>
#include "LvglWidgetBase.h"
#include "LvglTheme.h"
#include "LvglStyle.h"

namespace ui {

class LvglDropdown : public LvglWidgetBase {
public:
    using EventCallback = std::function<void(lv_event_t*)>;

    LvglDropdown(lv_obj_t* parent,
                 const std::vector<std::string>& options = {},
                 EventCallback cb = nullptr,
                 lv_align_t align = LV_ALIGN_CENTER,
                 lv_coord_t x_ofs = 0,
                 lv_coord_t y_ofs = 0,
                 lv_coord_t width = 160)
        : LvglWidgetBase(parent), callback_(std::move(cb))
    {
        lvObj_ = lv_dropdown_create(parent);
        lv_obj_set_width(lvObj_, width);
        lv_obj_align(lvObj_, align, x_ofs, y_ofs);

        if (!options.empty())
            setOptions(options);

        lv_obj_add_event_cb(lvObj_, &LvglDropdown::event_trampoline, LV_EVENT_ALL, this);

        applyTheme();
    }

    void setOptions(const std::vector<std::string>& options) {
        std::string joined;
        for (size_t i = 0; i < options.size(); ++i) {
            joined += options[i];
            if (i + 1 < options.size()) joined += "\n";
        }
        lv_dropdown_set_options(lvObj_, joined.c_str());
    }

    void clearOptions() {
        lv_dropdown_clear_options(lvObj_);
    }

    void addOption(const std::string& option, uint16_t pos = LV_DROPDOWN_POS_LAST) {
        lv_dropdown_add_option(lvObj_, option.c_str(), pos);
    }

    uint16_t getSelected() const {
        return lv_dropdown_get_selected(lvObj_);
    }

    void setSelected(uint16_t index) {
        lv_dropdown_set_selected(lvObj_, index);
    }

    std::string getSelectedStr() const {
        char buf[128];
        lv_dropdown_get_selected_str(lvObj_, buf, sizeof(buf));
        return std::string(buf);
    }

    void setCallback(EventCallback cb) {
        callback_ = std::move(cb);
    }

    void applyTheme() override {
        auto theme = LvglTheme::active();
        if (!theme) return;

        const LvglStyle* main     = theme->get("dropdown.main");
        const LvglStyle* selected = theme->get("dropdown.selected");

        lv_obj_remove_style_all(lvObj_);

        if (main)     main->applyTo(lvObj_, LV_PART_MAIN);
        if (selected) selected->applyTo(lvObj_, LV_PART_SELECTED);
    }

    // static void registerDefaultStyles(LvglTheme& theme) {
    //     theme.defineStyle("dropdown.main")
    //         .bgColor(lv_color_hex(0x303030))
    //         .textColor(lv_color_white())
    //         .radius(6)
    //         .padAll(4);

    //     theme.defineStyle("dropdown.list")
    //         .bgColor(lv_color_hex(0x202020))
    //         .borderWidth(1)
    //         .borderColor(lv_color_hex(0x505050))
    //         .radius(6);

    //     theme.defineStyle("dropdown.selected")
    //         .bgColor(lv_color_hex(0x50C878))
    //         .textColor(lv_color_white());
    // }

private:
    static void event_trampoline(lv_event_t* e) {
        auto* self = static_cast<LvglDropdown*>(lv_event_get_user_data(e));
        if (self && self->callback_)
            self->callback_(e);
    }

    EventCallback callback_;
};

} // namespace ui

#endif
