#pragma once

#include "LvglTextArea.h"
#include "LvglWidgetBase.h"
#include <functional>
#include <lvgl.h>
#include <string>

namespace ui
{

  class LvglKeyboard : public LvglWidgetBase
  {
  public:
    LvglKeyboard(lv_obj_t *parent, EventCallback cb = nullptr)
        : LvglWidgetBase(lv_keyboard_create(parent)), callback_(std::move(cb))
    {
      lv_obj_add_flag(lvObj_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES / 3);
      lv_obj_align(lvObj_, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_obj_add_event_cb(lvObj_, &LvglKeyboard::event_trampoline, LV_EVENT_ALL, this);
    }

    void setTextArea(lv_obj_t *textArea)
    {
      lv_keyboard_set_textarea(lvObj_, textArea);
    }

    void clearTextArea()
    {
      lv_keyboard_set_textarea(lvObj_, nullptr);
    }

    void setCallback(EventCallback cb) { callback_ = std::move(cb); }

    void applyTheme() override
    {
    }

  private:
    static void event_trampoline(lv_event_t *e)
    {
      auto *self = static_cast<LvglKeyboard *>(lv_event_get_user_data(e));
      if (self && self->callback_)
        self->callback_(e);
    }

    lv_obj_t *label_ = nullptr;
    EventCallback callback_;
  };

} // namespace ui
