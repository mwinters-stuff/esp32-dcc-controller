#pragma once
#include "LvglWrapper.h"
#include "images/CustomImages.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class TurnTableListItem {
public:
  lv_obj_t *parentObj;
  size_t index;

  TurnTableListItem(lv_obj_t *parent, size_t index, int turntableId, std::string name)
      : parentObj(parent), index(index), turntableId(turntableId), name(std::move(name)) {
    lvObj = lv_list_add_btn_mode(parent, getImage(), getDisplayName().c_str(), LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    setStylePart(lvObj, "wifi.item", LV_PART_MAIN);
    setStylePart(lvObj, "wifi.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t *getLvObj() const { return lvObj; }

  std::string getDisplayName() const {
    return name; // + " (" + (thrown ? "Thrown" : "Closed") + ")";
  }

  lv_image_dsc_t const *getImage() const { return &turnoutopen; }

  int getTurntableId() const { return turntableId; }

private:
  lv_obj_t *lvObj;
  int turntableId;
  std::string name;
};
} // namespace display
