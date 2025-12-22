#pragma once

#include <lvgl.h>
#include <string>
#include "ui/LvglTheme.h"

namespace display {

  void setStyle(lv_obj_t *widget, const std::string &styleName);


  lv_obj_t* getActiveScreen();
  lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName = "", const lv_font_t* font = nullptr);
  lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_coord_t width, lv_coord_t height, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName = "", const lv_font_t* font = nullptr);

}