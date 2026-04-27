#pragma once
#include "LvglWrapper.h"
#include "images/CustomImages.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class TurnoutListItem {
public:
  lv_obj_t *parentObj;
  size_t index;

  TurnoutListItem(lv_obj_t *parent, size_t index, int turnoutId, std::string name, bool thrown)
      : parentObj(parent), index(index), turnoutId(turnoutId), name(std::move(name)), thrown(thrown) {
    lvObj = lv_list_add_btn_mode(parent, getImage(), getDisplayName().c_str(), LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_align(lvObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_height(lvObj, 48);
    lv_obj_set_style_margin_bottom(lvObj, 2, LV_PART_MAIN);
    setStylePart(lvObj, "list.item", LV_PART_MAIN);
    setStylePart(lvObj, "list.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t *getLvObj() const { return lvObj; }

  void updateThrown(bool thrown) {
    this->thrown = thrown;
    lv_list_set_btn_icon(lvObj, getImage());
  }

  std::string getDisplayName() const {
    return name; // + " (" + (thrown ? "Thrown" : "Closed") + ")";
  }

  lv_image_dsc_t const *getImage() const {
    if (thrown) {
      return &turnoutclosed;
    } else {
      return &turnoutopen;
    }
  }

  int getTurnoutId() const { return turnoutId; }

  bool isThrown() const { return thrown; }

private:
  lv_obj_t *lvObj;
  int turnoutId;
  std::string name;
  bool thrown = false;
};
} // namespace display
