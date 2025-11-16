#pragma once
#include "LvglWidgetBase.h"

namespace ui
{

  class LvglVerticalLayout : public LvglWidgetBase
  {
  public:
    LvglVerticalLayout(lv_obj_t *parent,
                       lv_coord_t width = LV_SIZE_CONTENT, lv_coord_t height = LV_SIZE_CONTENT)
        : LvglWidgetBase(lv_obj_create(parent))
    {

      // Default alignment: top-left
      lv_obj_align(lvObj_, LV_ALIGN_TOP_LEFT, 0, 0);

      // Flexible vertical layout
      lv_obj_set_flex_flow(lvObj_, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(lvObj_,
                            LV_FLEX_ALIGN_START,  // main axis (top to bottom)
                            LV_FLEX_ALIGN_START,  // cross axis (left)
                            LV_FLEX_ALIGN_START); // track alignment

      // Size
      lv_obj_set_size(lvObj_, width, height);

      // No borders, padding, or background
      lv_obj_set_style_bg_opa(lvObj_, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(lvObj_, 0, 0);
      lv_obj_set_style_outline_width(lvObj_, 0, 0);
      lv_obj_set_style_pad_all(lvObj_, 0, 0);
      lv_obj_set_style_pad_gap(lvObj_, 4, 0);
      lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLLABLE);
    }
  };

}