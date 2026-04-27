#pragma once
#include "ListItemBase.h"
#include "LvglWrapper.h"
#include "images/CustomImages.h"
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class TurntableListItem : public IListItem {
public:
  TurntableListItem(lv_obj_t *parent, size_t index, int turntableId, std::string name)
      : turntableId(turntableId), name(std::move(name)) {
    parentObj = parent;
    this->index = index;
    lvObj = lv_list_add_btn_mode(parent, getImage(), getDisplayName().c_str(), LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_align(lvObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_height(lvObj, 48);
    lv_obj_set_style_margin_bottom(lvObj, 2, LV_PART_MAIN);
    setStylePart(lvObj, "list.item.bold", LV_PART_MAIN);
  }

  lv_obj_t *getLvObj() const override { return lvObj; }

  std::string getDisplayName() const override {
    return name; // + " (" + (thrown ? "Thrown" : "Closed") + ")";
  }

  lv_image_dsc_t const *getImage() const override { return &turnoutopen; }

  int getId() const override { return turntableId; }

private:
  lv_obj_t *lvObj;
  int turntableId;
  std::string name;
};

class TurntableIndexListItem : public IListItem {
public:
  TurntableIndexListItem(lv_obj_t *parent, size_t index, int turntableId, int indexId, std::string name)
      : turntableId(turntableId), indexId(indexId), name(std::move(name)) {
    parentObj = parent;
    this->index = index;
    lvObj = lv_list_add_btn_mode(parent, getImage(), getDisplayName().c_str(), LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_align(lvObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_height(lvObj, 48);
    lv_obj_set_style_margin_bottom(lvObj, 2, LV_PART_MAIN);
    setStylePart(lvObj, "list.item", LV_PART_MAIN);
    setStylePart(lvObj, "list.item.selected", LV_STATE_CHECKED);
    lv_obj_set_style_pad_left(lvObj, 50, LV_PART_MAIN);
  }

  lv_obj_t *getLvObj() const override { return lvObj; }

  std::string getDisplayName() const override {
    return name; // + " (" + (thrown ? "Thrown" : "Closed") + ")";
  }

  lv_image_dsc_t const *getImage() const override { return &turnoutopen; }

  int getTurntableId() const { return turntableId; }
  int getId() const override { return indexId; }

private:
  lv_obj_t *lvObj;
  int turntableId;
  int indexId;
  std::string name;
};

} // namespace display
