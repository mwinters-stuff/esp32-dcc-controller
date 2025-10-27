#ifndef _LVGL_LABEL_H
#define _LVGL_LABEL_H

#include <lvgl.h>
#include <string>

namespace ui{
class LvglLabel {
public:
    LvglLabel(lv_obj_t* parent,
              const std::string& text,
              lv_align_t align = LV_ALIGN_CENTER,
              lv_coord_t x_ofs = 0,
              lv_coord_t y_ofs = 0,
              const lv_font_t* font = nullptr)
    {
        label_ = lv_label_create(parent);
        lv_label_set_text(label_, text.c_str());
        lv_obj_align(label_, align, x_ofs, y_ofs);

        if (font) {
            lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);
        }else{
          lv_obj_set_style_text_font(label_, LV_FONT_DEFAULT, LV_PART_MAIN);
        }
    }

    lv_obj_t* getLvObj() const { return label_; }

    void setText(const std::string& text) {
        lv_label_set_text(label_, text.c_str());
    }

    void setAlignment(lv_align_t align, lv_coord_t x_ofs = 0, lv_coord_t y_ofs = 0) {
        lv_obj_align(label_, align, x_ofs, y_ofs);
    }

    void setFont(const lv_font_t* font) {
        lv_obj_set_style_text_font(label_, font, LV_PART_MAIN);
    }

    void setColor(lv_color_t color) {
        lv_obj_set_style_text_color(label_, color, LV_PART_MAIN);
    }

private:
    lv_obj_t* label_ = nullptr;
};
};

#endif