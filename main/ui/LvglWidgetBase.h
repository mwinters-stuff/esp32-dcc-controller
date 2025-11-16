#pragma once

#include "LvglStyle.h"
#include "LvglTheme.h"
#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglWidgetBase
  {
  public:
    using EventCallback = std::function<void(lv_event_t *)>;

    explicit LvglWidgetBase(lv_obj_t *obj, std::string styleKey = "")
        : lvObj_(obj), styleKey_(std::move(styleKey))
    {
      if (auto theme = LvglTheme::active())
      {
        applyTheme();
      }
    }

    virtual ~LvglWidgetBase() = default;

    lv_obj_t *lvObj() const { return lvObj_; }
    operator lv_obj_t *() { return lvObj_; }

    // Reapply current theme (for runtime theme switching)
    virtual void applyTheme()
    {
      auto theme = LvglTheme::active();
      if (!theme || styleKey_.empty())
        return;

      const auto &style = theme->style(styleKey_);
      style.applyTo(lvObj_);

      // Smooth transition for visual polish
      static const lv_style_prop_t props[] = {
          LV_STYLE_BG_COLOR,
          LV_STYLE_TEXT_COLOR,
          LV_STYLE_BORDER_COLOR,
          LV_STYLE_SHADOW_COLOR,
          (lv_style_prop_t)0};

      static lv_style_transition_dsc_t trans;
      static bool initialized = false;
      if (!initialized)
      {
        lv_style_transition_dsc_init(&trans, props, lv_anim_path_ease_in_out, 300, 0, nullptr);
        initialized = true;
      }

      lv_obj_set_style_transition(lvObj_, &trans, LV_PART_MAIN);
    }

    // Change which style key this widget uses
    void setStyleKey(const std::string &key)
    {
      styleKey_ = key;
      applyTheme();
    }

    const std::string &styleKey() const { return styleKey_; }

    virtual void setStyle(const std::string &styleName)
    {
      auto theme = LvglTheme::active();
      auto s = theme->get(styleName);
      if (s)
      {
        s->applyTo(lvObj_);
      }
    }

    void setAlignment(lv_align_t align, lv_coord_t x_ofs = 0, lv_coord_t y_ofs = 0)
    {
      if (lvObj_)
        lv_obj_align(lvObj_, align, x_ofs, y_ofs);
    }

    void setSize(lv_coord_t width, lv_coord_t height)
    {
      if (lvObj_)
        lv_obj_set_size(lvObj_, width, height);
    }

    void setFont(const lv_font_t *font)
    {
      if (lvObj_)
        lv_obj_set_style_text_font(lvObj_, font, LV_PART_MAIN);
    }

    void setColor(lv_color_t color)
    {
      if (lvObj_)
        lv_obj_set_style_text_color(lvObj_, color, LV_PART_MAIN);
    }

    void setVisible(bool visible)
    {
      if (lvObj_)
      {
        if (visible)
        {
          lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
          lv_obj_add_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
        }
      }
    }

    void setEnabled(bool enabled)
    {
      if (lvObj_)
      {
        if (enabled)
          lv_obj_clear_state(lvObj_, LV_STATE_DISABLED);
        else
          lv_obj_add_state(lvObj_, LV_STATE_DISABLED);
      }
    }

  protected:
    lv_obj_t *lvObj_ = nullptr;
    std::string styleKey_;
  };

} // namespace ui
