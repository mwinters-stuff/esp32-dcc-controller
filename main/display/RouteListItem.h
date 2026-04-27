#pragma once
#include "LvglWrapper.h"
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class RouteListItem {
public:
  lv_obj_t *parentObj;
  size_t index;

  RouteListItem(lv_obj_t *parent, size_t index, int routeId, std::string name, char type)
      : parentObj(parent), index(index), routeId(routeId), name(std::move(name)), type(type) {
    lvObj = lv_list_add_btn_mode(parent, getIcon(), getDisplayName().c_str(), LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_align(lvObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_height(lvObj, 48);
    lv_obj_set_style_margin_bottom(lvObj, 2, LV_PART_MAIN);
    setStylePart(lvObj, "list.item", LV_PART_MAIN);
    setStylePart(lvObj, "list.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t *getLvObj() const { return lvObj; }

  std::string getDisplayName() const {
    if (name.empty()) {
      return std::string("Route ") + std::to_string(routeId);
    }
    return name;
  }

  const char *getIcon() const { return (type == 'A') ? LV_SYMBOL_LOOP : LV_SYMBOL_PLAY; }

  int getRouteId() const { return routeId; }

private:
  lv_obj_t *lvObj = nullptr;
  int routeId;
  std::string name;
  char type;
};

} // namespace display
