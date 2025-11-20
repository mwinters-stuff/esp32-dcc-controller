#pragma once
#include <lvgl.h>
#include <memory>
#include <string>
#include "LvglWidgetBase.h"

namespace ui
{
  class LvglListItem : public LvglWidgetBase, public std::enable_shared_from_this<LvglListItem>
  {
  public:

    LvglListItem(lv_obj_t *parent, size_t index, const std::string &icon, const std::string &text)
        : LvglWidgetBase(lv_list_add_btn(parent, icon.c_str(), text.c_str())), parentObj_(parent), index_(index)
    {
      lv_obj_add_event_cb(lvObj_, &LvglListItem::event_trampoline, LV_EVENT_CLICKED, this);
      applyTheme();
    }

    virtual ~LvglListItem() = default;

    std::string getText() const
    {
      return lv_list_get_btn_text(parentObj_, lvObj_);
    }

    virtual void onClicked(size_t index)
    {
      // Derived classes can override for custom behavior
    }

    size_t index() const { return index_; }
    
    static void resetItems(){
      currentButton = nullptr;
      currentItem = nullptr;
    }

    void applyTheme()
    {
      static lv_style_t style_checked;
      static bool initialized = false;

      auto theme = ui::LvglTheme::active();
      if (!theme)
        return;

      const ui::LvglStyle *style = theme->get("wifi.item");
      if (style)
        style->applyTo(lvObj_, LV_PART_MAIN);


      if (!initialized)
      {
        lv_style_init(&style_checked);
        lv_style_set_bg_color(&style_checked, lv_color_hex(0x3399FF));
        lv_style_set_bg_opa(&style_checked, LV_OPA_50);
        lv_style_set_border_color(&style_checked, lv_color_hex(0x0066CC));
        lv_style_set_border_width(&style_checked, 2);
        initialized = true;
      }
      lv_obj_add_style(lvObj_, &style_checked, LV_STATE_CHECKED);
      lv_obj_set_height(lvObj_, 40);
    }

    lv_obj_t *parentObj_ = nullptr;
    static lv_obj_t *currentButton;
    static std::shared_ptr<LvglListItem> currentItem;
  private:
    size_t index_ = 0;

    static void event_trampoline(lv_event_t *e)
    {
      auto btn = static_cast<LvglListItem *>(lv_event_get_user_data(e))->shared_from_this();
      lv_event_code_t code = lv_event_get_code(e);

      if (code == LV_EVENT_CLICKED)
      {
        if (currentButton == btn->lvObj_)
        {
          currentButton = NULL;
          currentItem = nullptr;
        }
        else
        {
          currentButton = btn->lvObj_;
          currentItem = btn;
        }

        uint32_t i;
        for (i = 0; i < lv_obj_get_child_cnt(btn->parentObj_); i++)
        {
          lv_obj_t *child = lv_obj_get_child(btn->parentObj_, i);
          if (child == currentButton)
          {
            lv_obj_add_state(child, LV_STATE_CHECKED);
          }
          else
          {
            lv_obj_clear_state(child, LV_STATE_CHECKED);
          }
        }
        btn->onClicked(btn->index_);
      }
    }
  };

} // namespace ui