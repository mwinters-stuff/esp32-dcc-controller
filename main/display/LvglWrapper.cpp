#include "LvglWrapper.h"

namespace display {

void setStyle(lv_obj_t *widget, const std::string &styleName) {
  auto theme = ui::LvglTheme::active();
  auto s = theme->get(styleName);
  if (s) {
    s->applyTo(widget);
  }
}

void setStylePart(lv_obj_t *widget, const std::string &styleName, lv_part_t part) {
  auto theme = ui::LvglTheme::active();
  auto s = theme->get(styleName);
  if (s) {
    s->applyTo(widget, part);
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
                     lv_coord_t x_ofs, lv_coord_t y_ofs, const std::string &styleName, const lv_font_t *font) {
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

lv_obj_t *makeListView(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height,
                       const std::string &styleName) {
  lv_obj_t *list = lv_list_create(parent);
  lv_obj_align(list, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_size(list, width, height);
  setStyle(list, styleName);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

  return list;
}

void lv_list_set_btn_text(lv_obj_t *btn, const char *text) {

  uint32_t i;
  for (i = 0; i < lv_obj_get_child_cnt(btn); i++) {
    lv_obj_t *child = lv_obj_get_child(btn, i);
    if (lv_obj_check_type(child, &lv_label_class)) {
      lv_label_set_text(child, text);
    }
  }
}

lv_obj_t *makeSpinner(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size, uint16_t speed) {
  lv_obj_t *spinner = lv_spinner_create(parent, speed, 60);
  lv_obj_set_size(spinner, size, size);
  lv_obj_align(spinner, LV_ALIGN_CENTER, x, y);
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  return spinner;
}

lv_obj_t *makeVerticalLayout(lv_obj_t *parent, lv_coord_t width, lv_coord_t height) {
  lv_obj_t *layout = lv_obj_create(parent);
  // Default alignment: top-left
  lv_obj_align(layout, LV_ALIGN_TOP_LEFT, 0, 0);

  // Flexible vertical layout
  lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(layout,
                        LV_FLEX_ALIGN_START,  // main axis (top to bottom)
                        LV_FLEX_ALIGN_START,  // cross axis (left)
                        LV_FLEX_ALIGN_START); // track alignment

  // Size
  lv_obj_set_size(layout, width, height);

  // No borders, padding, or background
  lv_obj_set_style_bg_opa(layout, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(layout, 0, 0);
  lv_obj_set_style_outline_width(layout, 0, 0);
  lv_obj_set_style_pad_all(layout, 0, 0);
  lv_obj_set_style_pad_gap(layout, 4, 0);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);

  return layout;
}

lv_obj_t *makeHorizontalLayout(lv_obj_t *parent, lv_coord_t width, lv_coord_t height) {
  lv_obj_t *layout = lv_obj_create(parent);

  // Default alignment: top-left
  lv_obj_align(layout, LV_ALIGN_TOP_LEFT, 0, 0);

  // Flexible horizontal layout
  lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(layout,
                        LV_FLEX_ALIGN_START,  // main axis (left to right)
                        LV_FLEX_ALIGN_CENTER, // cross axis (vertically centered)
                        LV_FLEX_ALIGN_START); // track alignment

  // Size
  lv_obj_set_size(layout, width, height);

  // Remove outlines, borders, and padding for a clean look
  lv_obj_set_style_bg_opa(layout, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(layout, 0, 0);
  lv_obj_set_style_outline_width(layout, 0, 0);
  lv_obj_set_style_pad_all(layout, 0, 0);
  lv_obj_set_style_pad_gap(layout, 4, 0);
  lv_obj_clear_flag(layout, LV_OBJ_FLAG_SCROLLABLE);

  return layout;
}

lv_obj_t *makeTextArea(lv_obj_t *parent, const std::string &placeholder, bool password, bool one_line) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_obj_set_flex_grow(ta, 1);

  // Configure behavior
  lv_textarea_set_placeholder_text(ta, placeholder.c_str());
  lv_textarea_set_one_line(ta, one_line);
  lv_textarea_set_password_mode(ta, password);

  // Default style (optional)
  lv_obj_set_style_pad_all(ta, 4, 0);
  return ta;
}

lv_obj_t *makeButtonSymbol(lv_obj_t *parent, const std::string &symbol, const lv_coord_t width, const lv_coord_t height,
                           bool checkable) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 40, 40);

  lv_obj_t *eye_lbl = lv_label_create(btn);
  lv_label_set_text(eye_lbl, symbol.c_str());
  lv_obj_center(eye_lbl);

  if (checkable) {
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
  }
  return btn;
}

lv_obj_t *makeKeyboard(lv_obj_t *parent) {
  lv_obj_t *keyboard = lv_keyboard_create(parent);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(keyboard, LV_HOR_RES, LV_VER_RES / 3);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  return keyboard;
}

lv_obj_t *makeTabView(lv_obj_t *parent, lv_align_t align, int x_ofs, int y_ofs, int width, int height)
{
    lv_obj_t *tabview = lv_tabview_create(parent, LV_DIR_TOP, 40);
    lv_obj_set_size(tabview, width, height);
    lv_obj_align(tabview, align, x_ofs, y_ofs);
    return tabview;
}

} // namespace display