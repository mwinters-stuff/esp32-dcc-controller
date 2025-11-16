#pragma once

#include "LvglWidgetBase.h"
#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglSpinner : public LvglWidgetBase
  {
  public:
    LvglSpinner(lv_obj_t *parent)
        : LvglWidgetBase(lv_spinner_create(parent, 1000, 60))
    {
      lv_obj_center(lvObj_);
      lv_obj_set_size(lvObj_, 60, 60);
      lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
      // Base constructor auto-applies theme
    }

    void showSpinner()
    {
      lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
    }

    void hideSpinner()
    {
      lv_obj_add_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
    }

    void applyTheme() override
    {
      //   auto theme = LvglTheme::active();
      //   if (!theme)
      //     return;

      //   const LvglStyle *main = theme->get("spinner.main");

      //   // lv_obj_remove_style_all(lvObj_);

      //   if (main)
      //     main->applyTo(lvObj_, LV_PART_MAIN);
    }
  };

} // namespace ui
