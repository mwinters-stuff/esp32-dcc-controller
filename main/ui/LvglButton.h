#ifndef _LVGL_BUTTON_H
#define _LVGL_BUTTON_H

#include <lvgl.h>
#include <functional>
#include <string>
#include "LvglWidgetBase.h"
#include "LvglTheme.h"

namespace ui {

class LvglButton : public LvglWidgetBase {
public:
    using EventCallback = std::function<void(lv_event_t*)>;

    LvglButton(lv_obj_t* parent,
               const std::string& text,
               EventCallback cb = nullptr,
               lv_coord_t width = 200,
               lv_coord_t height = 48,
               lv_align_t align = LV_ALIGN_CENTER,
               lv_coord_t x_ofs = 0,
               lv_coord_t y_ofs = 0,
               const lv_font_t* font = nullptr)
        : LvglWidgetBase(lv_btn_create(parent), "button"),
          callback_(std::move(cb))
    {
        lv_obj_set_size(lvObj_, width, height);

        // ✅ LVGL 9 alignment
        lv_obj_set_align(lvObj_, align);
        if (x_ofs || y_ofs) lv_obj_set_pos(lvObj_, x_ofs, y_ofs);

        lv_obj_add_event_cb(lvObj_, &LvglButton::event_trampoline, LV_EVENT_ALL, this);

        label_ = lv_label_create(lvObj_);
        lv_label_set_text(label_, text.c_str());
        lv_obj_center(label_);

        if (font)
            lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);

        // Base constructor auto-applies theme
    }

    void setText(const std::string& text) { lv_label_set_text(label_, text.c_str()); }
    void setCallback(EventCallback cb) { callback_ = std::move(cb); }

    void applyTheme() override {
        auto theme = LvglTheme::active();
        if (!theme) return;

        const LvglStyle* main     = theme->get("button.main");
        const LvglStyle* pressed  = theme->get("button.pressed");
        const LvglStyle* disabled = theme->get("button.disabled");

        // ✅ Clear existing styles (LVGL 9 same)
        lv_obj_remove_style_all(lvObj_);

        // Apply base style
        if (main) main->applyTo(lvObj_, LV_PART_MAIN);

        // Apply pressed variant (state overlay)
        if (pressed) pressed->applyTo(lvObj_, LV_PART_MAIN, LV_STATE_PRESSED);

        // Apply disabled variant
        if (disabled) disabled->applyTo(lvObj_, LV_PART_MAIN, LV_STATE_DISABLED);

        // Apply to label as well (theme consistency)
        const LvglStyle* labelStyle = theme->get("label.main");
        if (labelStyle) labelStyle->applyTo(label_, LV_PART_MAIN);
    }

private:
    static void event_trampoline(lv_event_t* e) {
        auto* self = static_cast<LvglButton*>(lv_event_get_user_data(e));
        if (self && self->callback_) self->callback_(e);
    }

    lv_obj_t* label_ = nullptr;
    EventCallback callback_;
};

} // namespace ui

#endif
