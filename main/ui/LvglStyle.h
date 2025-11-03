#ifndef _LVGL_STYLE_H
#define _LVGL_STYLE_H

#include <lvgl.h>

namespace ui {

class LvglStyle {
public:
    LvglStyle() {
        lv_style_init(&style_);
    }

    LvglStyle(const LvglStyle&) = delete;
    LvglStyle& operator=(const LvglStyle&) = delete;

    LvglStyle(LvglStyle&& other) noexcept {
        lv_style_init(&style_);
        moved_ = false;
        copyCacheFrom(other);
    }

    LvglStyle& operator=(LvglStyle&& other) noexcept {
        if (this != &other) {
            lv_style_reset(&style_);
            lv_style_init(&style_);
            copyCacheFrom(other);
        }
        return *this;
    }

    ~LvglStyle() {
        if (!moved_) {
            lv_style_reset(&style_);
        }
    }

    // clone properties
    LvglStyle& copyFrom(const LvglStyle& other) {
        copyCacheFrom(other);
        return *this;
    }

    //
    // Fluent property setters
    //
    LvglStyle& bgColor(lv_color_t color) {
        bg_color_ = color;
        has_bg_color_ = true;
        lv_style_set_bg_color(&style_, color);
        return *this;
    }

    LvglStyle& bgOpacity(lv_opa_t opa) {
        bg_opa_ = opa;
        has_bg_opa_ = true;
        lv_style_set_bg_opa(&style_, opa);
        return *this;
    }

    LvglStyle& textColor(lv_color_t color) {
        text_color_ = color;
        has_text_color_ = true;
        lv_style_set_text_color(&style_, color);
        return *this;
    }

    LvglStyle& textFont(const lv_font_t* font) {
        font_ = font;
        has_font_ = true;
        lv_style_set_text_font(&style_, font);
        return *this;
    }

    LvglStyle& borderColor(lv_color_t color) {
        border_color_ = color;
        has_border_color_ = true;
        lv_style_set_border_color(&style_, color);
        return *this;
    }

    LvglStyle& borderWidth(lv_coord_t width) {
        border_width_ = width;
        has_border_width_ = true;
        lv_style_set_border_width(&style_, width);
        return *this;
    }

    LvglStyle& radius(lv_coord_t rad) {
        radius_ = rad;
        has_radius_ = true;
        lv_style_set_radius(&style_, rad);
        return *this;
    }

    LvglStyle& padAll(lv_coord_t pad) {
        pad_all_ = pad;
        has_pad_all_ = true;
        lv_style_set_pad_all(&style_, pad);
        return *this;
    }

    LvglStyle& shadowWidth(lv_coord_t width) {
        shadow_width_ = width;
        has_shadow_width_ = true;
        lv_style_set_shadow_width(&style_, width);
        return *this;
    }

    LvglStyle& shadowColor(lv_color_t color) {
        shadow_color_ = color;
        has_shadow_color_ = true;
        lv_style_set_shadow_color(&style_, color);
        return *this;
    }

    //
    // Apply to an LVGL object
    //
    void applyToPart(lv_obj_t* obj, lv_part_t part = LV_PART_MAIN) const {
        if (!obj) return;

        // In LVGL 9: states are no longer part of the selector argument.
        // Add style to the given part only.
        lv_obj_add_style(obj, const_cast<lv_style_t*>(&style_), part);
    }

    void applyTo(lv_obj_t* obj,
                lv_part_t part = LV_PART_MAIN,
                lv_state_t state = LV_STATE_DEFAULT) const
    {
        if (!obj) return;

        // Build a selector (lv_style_selector_t is an OR-ed bitmask of part + state)
        lv_style_selector_t sel =
            static_cast<lv_style_selector_t>(part) |
            static_cast<lv_style_selector_t>(state);

        lv_obj_add_style(obj, const_cast<lv_style_t*>(&style_), sel);
    }

private:
    void copyCacheFrom(const LvglStyle& other) {
        // copy cached parameters
        *this = LvglStyle(); // re-init
        if (other.has_bg_color_) bgColor(other.bg_color_);
        if (other.has_bg_opa_) bgOpacity(other.bg_opa_);
        if (other.has_text_color_) textColor(other.text_color_);
        if (other.has_font_) textFont(other.font_);
        if (other.has_border_color_) borderColor(other.border_color_);
        if (other.has_border_width_) borderWidth(other.border_width_);
        if (other.has_radius_) radius(other.radius_);
        if (other.has_pad_all_) padAll(other.pad_all_);
        if (other.has_shadow_color_) shadowColor(other.shadow_color_);
        if (other.has_shadow_width_) shadowWidth(other.shadow_width_);
    }

    lv_style_t style_{};
    bool moved_ = false;

    // cached properties
    lv_color_t bg_color_;
    lv_color_t text_color_;
    lv_color_t border_color_;
    lv_color_t shadow_color_;
    const lv_font_t* font_ = nullptr;
    lv_coord_t border_width_ = 0;
    lv_coord_t radius_ = 0;
    lv_coord_t pad_all_ = 0;
    lv_coord_t shadow_width_ = 0;
    lv_opa_t bg_opa_ = LV_OPA_COVER;

    bool has_bg_color_ = false;
    bool has_text_color_ = false;
    bool has_border_color_ = false;
    bool has_shadow_color_ = false;
    bool has_font_ = false;
    bool has_border_width_ = false;
    bool has_radius_ = false;
    bool has_pad_all_ = false;
    bool has_shadow_width_ = false;
    bool has_bg_opa_ = false;
};

} // namespace ui

#endif

/* Example
LvglStyle base;
base.bgColor(lv_color_hex(0x202020))
    .textColor(lv_color_white())
    .borderWidth(2)
    .borderColor(lv_color_hex(0x404040));

LvglStyle pressed;
pressed.copyFrom(base)
       .bgColor(lv_color_hex(0x303030))
       .shadowWidth(8);

pressed.applyTo(my_button);

*/