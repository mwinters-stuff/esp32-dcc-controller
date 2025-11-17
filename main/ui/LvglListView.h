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

    static void lv_list_set_btn_text(lv_obj_t * btn, const char * text)
    {

      uint32_t i;
      for(i = 0; i < lv_obj_get_child_cnt(btn); i++) {
        lv_obj_t * child = lv_obj_get_child(btn, i);
        if(lv_obj_check_type(child, &lv_label_class)) {
            lv_label_set_text(child, text);
        }
    }
  }


    // void clear(){
    //   // lv_deletelist_clean(lvObj_);
    // }
    
    // void addItem(const std::string &text, std::function<void(lv_event_t *)> onClick){
    //   lv_obj_t *btn = lv_list_add_btn(lvObj_, nullptr, text.c_str());
    //   if (onClick){
    //     lv_obj_add_event_cb(btn, [](lv_event_t *e){
    //       auto func = static_cast<std::function<void(lv_event_t *)> *>(lv_event_get_user_data(e));
    //       (*func)(e);
    //     }, LV_EVENT_ALL, new std::function<void(lv_event_t *)>(onClick));
    //   }
    // }
  };

} // namespace ui
