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
        : LvglWidgetBase(lv_dropdown_create(parent), "dropdown"),
          callback_(std::move(cb))
    {
        lv_obj_set_width(lvObj_, width);

        // ✅ Alignment API change in LVGL 9
        lv_obj_set_align(lvObj_, align);
        if (x_ofs || y_ofs) lv_obj_set_pos(lvObj_, x_ofs, y_ofs);

        if (!options.empty())
            setOptions(options);

        lv_obj_add_event_cb(lvObj_, &LvglDropdown::event_trampoline, LV_EVENT_ALL, this);

        applyTheme();
    }

    //
    // 🔹 Options handling
    //
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

    //
    // 🔹 Theme integration
    //
    void applyTheme() override {
        auto theme = LvglTheme::active();
        if (!theme) return;

        const LvglStyle* main     = theme->get("dropdown.main");
        const LvglStyle* selected = theme->get("dropdown.selected");
        const LvglStyle* list     = theme->get("dropdown.list"); // optional part

        lv_obj_remove_style_all(lvObj_);

        if (main)     main->applyTo(lvObj_, LV_PART_MAIN);
        if (selected) selected->applyTo(lvObj_, LV_PART_SELECTED);

        // ✅ LVGL 9: dropdown’s list is a child object, style it too if available
        if (list) {
            lv_obj_t* list_obj = lv_dropdown_get_list(lvObj_);
            if (list_obj) list->applyTo(list_obj, LV_PART_MAIN);
        }
    }

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
