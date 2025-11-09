#pragma once

#include "LvglStyle.h"
#include "LvglTheme.h"
#include "LvglWidgetBase.h"
#include <functional>
#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglCheckbox : public LvglWidgetBase
  {
  public:
    using EventCallback = std::function<void(lv_event_t *)>;

    LvglCheckbox(lv_obj_t *parent,
                 const std::string &text = "",
                 EventCallback cb = nullptr,
                 lv_align_t align = LV_ALIGN_CENTER,
                 lv_coord_t x_ofs = 0,
                 lv_coord_t y_ofs = 0)
        : LvglWidgetBase(parent), callback_(std::move(cb))
    {
      lvObj_ = lv_checkbox_create(parent);
      lv_obj_align(lvObj_, align, x_ofs, y_ofs);

      if (!text.empty())
        lv_checkbox_set_text(lvObj_, text.c_str());

      lv_obj_add_event_cb(lvObj_, &LvglCheckbox::event_trampoline, LV_EVENT_ALL, this);

      applyTheme(); // apply immediately
    }

    // --- Checkbox state helpers ---
    bool isChecked() const
    {
      return lv_obj_has_state(lvObj_, LV_STATE_CHECKED);
    }

    void setChecked(bool checked)
    {
      if (checked)
        lv_obj_add_state(lvObj_, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(lvObj_, LV_STATE_CHECKED);
    }

    void toggle()
    {
      setChecked(!isChecked());
    }

    void setText(const std::string &text)
    {
      lv_checkbox_set_text(lvObj_, text.c_str());
    }

    std::string text() const
    {
      return lv_checkbox_get_text(lvObj_) ? lv_checkbox_get_text(lvObj_) : "";
    }

    void setCallback(EventCallback cb)
    {
      callback_ = std::move(cb);
    }

    // --- Theming ---
    void applyTheme() override
    {
      auto theme = LvglTheme::active();
      if (!theme)
        return;

      const LvglStyle *main = theme->get("checkbox.main");
      const LvglStyle *checked = theme->get("checkbox.checked");
      const LvglStyle *disabled = theme->get("checkbox.disabled");

      lv_obj_remove_style_all(lvObj_);

      // Base style for the whole checkbox
      if (main)
        main->applyTo(lvObj_, LV_PART_MAIN);

      // Checked indicator box
      if (checked)
        checked->applyTo(lvObj_, LV_PART_INDICATOR | LV_STATE_CHECKED);

      // Disabled
      if (disabled)
        disabled->applyTo(lvObj_, LV_PART_MAIN | LV_STATE_DISABLED);
    }

  private:
    static void event_trampoline(lv_event_t *e)
    {
      auto *self = static_cast<LvglCheckbox *>(lv_event_get_user_data(e));
      if (self && self->callback_)
        self->callback_(e);
    }

    EventCallback callback_;
  };

} // namespace ui
