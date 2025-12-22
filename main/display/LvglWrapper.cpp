#include "LvglWrapper.h"

namespace display {

void setStyle(lv_obj_t *widget, const std::string &styleName) {
  auto theme = ui::LvglTheme::active();
  auto s = theme->get(styleName);
  if (s) {
    s->applyTo(widget);
  }
}

lv_obj_t *getActiveScreen() { return lv_scr_act(); }

lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs,
                    const std::string &styleName, const lv_font_t *font) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_align(label, align, x_ofs, y_ofs);
  setStyle(label, styleName);
  if (font != nullptr) {
    lv_obj_set_style_text_font(label, font, 0);
  }
  return label;
}

lv_obj_t *makeButton(lv_obj_t *parent, const char *text, lv_coord_t width, lv_coord_t height, lv_align_t align,
                     lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName,
                     const lv_font_t *font) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_align(btn, align, x_ofs, y_ofs);
  setStyle(btn, styleName);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  if (font != nullptr) {
    lv_obj_set_style_text_font(label, font, 0);
  }
  return btn;
}

} // namespace display