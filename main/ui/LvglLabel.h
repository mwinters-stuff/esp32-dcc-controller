#pragma once

#include "LvglWidgetBase.h"
#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglLabel : public LvglWidgetBase
  {
  public:
    LvglLabel(lv_obj_t *parent,
              const std::string &text,
              lv_align_t align = LV_ALIGN_CENTER,
              lv_coord_t x_ofs = 0,
              lv_coord_t y_ofs = 0,
              const lv_font_t *font = nullptr)
        : LvglWidgetBase(lv_label_create(parent), "label")
    {
      lv_label_set_text(lvObj_, text.c_str());
      lv_obj_align(lvObj_, align, x_ofs, y_ofs);

      if (font)
        lv_obj_set_style_text_font(lvObj_, font, LV_PART_MAIN);

      // Base constructor auto-applies theme
    }

    void setText(const std::string &text)
    {
      lv_label_set_text(lvObj_, text.c_str());
    }

    void setTextFmt(const char *fmt, ...)
    {
      if (!fmt)
      {
        lv_label_set_text(lvObj_, "");
        return;
      }

      va_list ap;
      va_start(ap, fmt);

      lv_label_set_text_fmt(lvObj_, fmt, ap);

      // // Determine required size
      // va_list ap_copy;
      // va_copy(ap_copy, ap);
      // int needed = vsnprintf(nullptr, 0, fmt, ap_copy);
      // va_end(ap_copy);

      // if (needed < 0) {
      //   va_end(ap);
      //   return;
      // }

      // std::string buf;
      // buf.resize(static_cast<size_t>(needed) + 1);
      // vsnprintf(buf.data(), buf.size(), fmt, ap);
      va_end(ap);

      // // resize to exclude the trailing null for storage correctness
      // buf.resize(static_cast<size_t>(needed));
      // lv_label_set_text(lvObj_, buf.c_str());
    }

    void applyTheme() override
    {
      auto theme = LvglTheme::active();
      if (!theme)
        return;

      const LvglStyle *main = theme->get("label.main");

      // lv_obj_remove_style_all(lvObj_);

      if (main)
        main->applyTo(lvObj_, LV_PART_MAIN);
    }
  };

} // namespace ui
