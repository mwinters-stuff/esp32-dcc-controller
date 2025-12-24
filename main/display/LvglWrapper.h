#pragma once

#include <lvgl.h>
#include <string>
#include "ui/LvglTheme.h"

namespace display {

  void setStyle(lv_obj_t *widget, const std::string &styleName);
  void setStylePart(lv_obj_t *widget, const std::string &styleName, lv_part_t part);


  lv_obj_t* getActiveScreen();
  lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName = "", const lv_font_t* font = nullptr);
  lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_coord_t width, lv_coord_t height, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName = "", const lv_font_t* font = nullptr);
  lv_obj_t* makeListView(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height, const std::string &styleName = "");
  lv_obj_t* makeSpinner(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size, uint16_t speed = 1000);
  lv_obj_t *makeVerticalLayout(lv_obj_t *parent, lv_coord_t width = LV_SIZE_CONTENT,lv_coord_t height = LV_SIZE_CONTENT);
  lv_obj_t *makeHorizontalLayout(lv_obj_t *parent, lv_coord_t width = LV_SIZE_CONTENT,lv_coord_t height = LV_SIZE_CONTENT);
  lv_obj_t *makeTextArea(lv_obj_t *parent, const std::string &placeholder = "", bool password = false, bool one_line = true);
  lv_obj_t* makeButtonSymbol(lv_obj_t* parent, const std::string &symbol, const lv_coord_t width = 40, const lv_coord_t height = 40, bool checkable = false);
  lv_obj_t* makeKeyboard(lv_obj_t* parent);
  lv_obj_t* makeTabView(lv_obj_t* parent, lv_align_t align, int x_ofs, int y_ofs, int width, int height);
  
  void lv_list_set_btn_text(lv_obj_t* btn, const char* text);

}