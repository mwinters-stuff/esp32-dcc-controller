#ifndef _LVGL_BUTTON_H
#define _LVGL_BUTTON_H

#include <lvgl.h>
#include <functional>
#include <string>

namespace ui{
class LvglButton {
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
        : callback_(std::move(cb))
    {
        // Create button
        btn_ = lv_btn_create(parent);
        lv_obj_set_size(btn_, width, height);
        lv_obj_align(btn_, align, x_ofs, y_ofs);

        // Register event handler (trampoline)
        lv_obj_add_event_cb(btn_, &LvglButton::event_trampoline, LV_EVENT_ALL, this);

        // Create label
        label_ = lv_label_create(btn_);
        lv_label_set_text(label_, text.c_str());
        lv_obj_center(label_);

        if (font) {
            lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);
        }else{
          lv_obj_set_style_text_font(label_, LV_FONT_DEFAULT, LV_PART_MAIN);
        }

    }

    lv_obj_t* button() const { return btn_; }
    lv_obj_t* label() const { return label_; }

    void setText(const std::string& text) {
        lv_label_set_text(label_, text.c_str());
    }

    void setEnabled(bool enabled) {
        if (enabled)
            lv_obj_clear_state(btn_, LV_STATE_DISABLED);
        else
            lv_obj_add_state(btn_, LV_STATE_DISABLED);
    }

    void setCallback(EventCallback cb) {
        callback_ = std::move(cb);
    }

    void setFont(const lv_font_t* font) {
        lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);
    }


private:
    static void event_trampoline(lv_event_t* e) {
        auto* self = static_cast<LvglButton*>(lv_event_get_user_data(e));
        if (self && self->callback_) {
            self->callback_(e);
        }
    }

    lv_obj_t* btn_ = nullptr;
    lv_obj_t* label_ = nullptr;
    EventCallback callback_;
};

};
#endif