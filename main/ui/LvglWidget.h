#pragma once

#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglWidget
  {
  public:
    explicit LvglWidget(lv_obj_t *obj = nullptr) : obj_(obj) {}
    virtual ~LvglWidget() = default;

    lv_obj_t *lvObj() const { return obj_; }

    void setAlignment(lv_align_t align, lv_coord_t x_ofs = 0, lv_coord_t y_ofs = 0)
    {
      if (obj_)
        lv_obj_align(obj_, align, x_ofs, y_ofs);
    }

    void setSize(lv_coord_t width, lv_coord_t height)
    {
      if (obj_)
        lv_obj_set_size(obj_, width, height);
    }

    void setFont(const lv_font_t *font)
    {
      if (obj_)
        lv_obj_set_style_text_font(obj_, font, LV_PART_MAIN);
    }

    void setColor(lv_color_t color)
    {
      if (obj_)
        lv_obj_set_style_text_color(obj_, color, LV_PART_MAIN);
    }

    void setVisible(bool visible)
    {
      if (obj_)
        lv_obj_add_flag(obj_, visible ? LV_OBJ_FLAG_HIDDEN : 0);
    }

    void setEnabled(bool enabled)
    {
      if (obj_)
      {
        if (enabled)
          lv_obj_clear_state(obj_, LV_STATE_DISABLED);
        else
          lv_obj_add_state(obj_, LV_STATE_DISABLED);
      }
    }

  protected:
    lv_obj_t *obj_ = nullptr;
  };

} // namespace ui
