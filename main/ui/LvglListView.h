#pragma once
#include "LvglStyle.h"
#include "LvglTheme.h"
#include "LvglWidgetBase.h"
#include <lvgl.h>
#include <memory>

namespace ui
{

  class LvglListView : public LvglWidgetBase
  {
  public:
    LvglListView(lv_obj_t *parent, uint16_t x, uint16_t y, uint16_t width = LV_PCT(100), uint16_t height = LV_PCT(100))
        : LvglWidgetBase(lv_list_create(parent))
    {
      // full width, allow scrolling vertically
      lv_obj_align(lvObj_, LV_ALIGN_TOP_LEFT, x, y);
      lv_obj_set_size(lvObj_, width, height);
      lv_obj_set_scroll_dir(lvObj_, LV_DIR_VER);
      lv_obj_set_scrollbar_mode(lvObj_, LV_SCROLLBAR_MODE_AUTO);
      applyTheme();
    }

    void applyTheme() override
    {
      auto theme = LvglTheme::active();
      if (!theme)
        return;
      const LvglStyle *style = theme->get("list.main");
      if (style)
        style->applyTo(lvObj_, LV_PART_MAIN);
    }
  };

} // namespace ui
