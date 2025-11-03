#ifndef _LVGL_LABEL_H
#define _LVGL_LABEL_H

#include <lvgl.h>
#include <string>
#include "LvglWidgetBase.h"

namespace ui {

class LvglLabel : public LvglWidgetBase {
public:
    LvglLabel(lv_obj_t* parent,
              const std::string& text,
              lv_align_t align = LV_ALIGN_CENTER,
              lv_coord_t x_ofs = 0,
              lv_coord_t y_ofs = 0,
              const lv_font_t* font = nullptr)
        : LvglWidgetBase(lv_label_create(parent), "label")
    {
        lv_label_set_text(lvObj_, text.c_str());
        lv_obj_align(lvObj_, align, x_ofs, y_ofs);

        if (font)
            lv_obj_set_style_text_font(lvObj_, font, LV_PART_MAIN);

        // Base constructor auto-applies theme
    }

    void setText(const std::string& text) {
        lv_label_set_text(lvObj_, text.c_str());
    }

    void applyTheme() override {
    auto theme = LvglTheme::active();
    if (!theme) return;

    const LvglStyle* main = theme->get("label.main");

    lv_obj_remove_style_all(lvObj_);

    if (main) main->applyTo(lvObj_, LV_PART_MAIN);
}
};

} // namespace ui

#endif
