#pragma once
#include "LvglWidgetBase.h"
#include <functional>
#include <string>

namespace ui
{

  class LvglButtonSymbol : public LvglWidgetBase
  {
  public:
    LvglButtonSymbol(lv_obj_t *parent,
                     const std::string &symbol,
                     const lv_coord_t width = 40,
                     const lv_coord_t height = 40,
                     bool checkable = false)
        : LvglWidgetBase(lv_btn_create(parent))
    {
      lv_obj_set_size(lvObj_, 40, 40);

      lv_obj_t *eye_lbl = lv_label_create(lvObj_);
      lv_label_set_text(eye_lbl, symbol.c_str());
      lv_obj_center(eye_lbl);

      if (checkable)
      {
        lv_obj_add_flag(lvObj_, LV_OBJ_FLAG_CHECKABLE);
      }

      lv_obj_add_event_cb(lvObj_, &LvglButtonSymbol::event_trampoline, LV_EVENT_ALL, this);
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
      auto *self = static_cast<LvglButtonSymbol *>(lv_event_get_user_data(e));
      if (self && self->callback_)
        self->callback_(e);
    }

    EventCallback callback_;
  };

}