#pragma once
#include "LvglWidgetBase.h"

namespace ui
{

  class LvglHorizontalLayout : public LvglWidgetBase
  {
  public:
    LvglHorizontalLayout(lv_obj_t *parent,
                         lv_coord_t width = LV_SIZE_CONTENT, lv_coord_t height = LV_SIZE_CONTENT)
        : LvglWidgetBase(lv_obj_create(parent))
    {

      // Default alignment: top-left
      lv_obj_align(lvObj_, LV_ALIGN_TOP_LEFT, 0, 0);

      // Flexible horizontal layout
      lv_obj_set_flex_flow(lvObj_, LV_FLEX_FLOW_ROW);
      lv_obj_set_flex_align(lvObj_,
                            LV_FLEX_ALIGN_START,  // main axis (left to right)
                            LV_FLEX_ALIGN_CENTER, // cross axis (vertically centered)
                            LV_FLEX_ALIGN_START); // track alignment

      // Size
      lv_obj_set_size(lvObj_, width, height);

      // Remove outlines, borders, and padding for a clean look
      lv_obj_set_style_bg_opa(lvObj_, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(lvObj_, 0, 0);
      lv_obj_set_style_outline_width(lvObj_, 0, 0);
      lv_obj_set_style_pad_all(lvObj_, 0, 0);
      lv_obj_set_style_pad_gap(lvObj_, 4, 0);
      lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLLABLE);
    }
  };

}