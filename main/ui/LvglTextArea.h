#pragma once
#include "LvglWidgetBase.h"
#include <functional>
#include <string>

namespace ui
{

  class LvglTextArea : public LvglWidgetBase
  {
  public:
    LvglTextArea(lv_obj_t *parent,
                 const std::string &placeholder = "",
                 bool password = false,
                 bool one_line = true)
        : LvglWidgetBase(lv_textarea_create(parent))
    {
      lv_obj_set_flex_grow(lvObj_, 1);

      // Configure behavior
      lv_textarea_set_placeholder_text(lvObj_, placeholder.c_str());
      lv_textarea_set_one_line(lvObj_, one_line);
      lv_textarea_set_password_mode(lvObj_, password);

      lv_obj_add_event_cb(lvObj_, &LvglTextArea::event_trampoline, LV_EVENT_ALL, this);

      // Default style (optional)
      lv_obj_set_style_pad_all(lvObj_, 4, 0);
    }

    void setPlaceholder(const std::string &text)
    {
      lv_textarea_set_placeholder_text(lvObj_, text.c_str());
    }

    void setPasswordMode(bool enable)
    {
      lv_textarea_set_password_mode(lvObj_, enable);
    }

    void setOneLine(bool enable)
    {
      lv_textarea_set_one_line(lvObj_, enable);
    }

    std::string getText() const
    {
      const char *txt = lv_textarea_get_text(lvObj_);
      return txt ? std::string(txt) : std::string();
    }

    void setText(const std::string &text)
    {
      lv_textarea_set_text(lvObj_, text.c_str());
    }

    void setCallback(EventCallback cb) { callback_ = std::move(cb); }

  private:
    static void event_trampoline(lv_event_t *e)
    {
      auto *self = static_cast<LvglTextArea *>(lv_event_get_user_data(e));
      if (self && self->callback_)
        self->callback_(e);
    }

    EventCallback callback_;
  };

}