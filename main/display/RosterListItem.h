#pragma once
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>
#include "LvglWrapper.h"
#include "images/CustomImages.h"

namespace display {

class RosterListItem {
public:
  lv_obj_t *parentObj;
  size_t index;

  RosterListItem(lv_obj_t *parent, size_t index, int address, std::string name): parentObj(parent), index(index), address(address), name(std::move(name)){
    lvObj = lv_list_add_btn_mode(parent, LV_SYMBOL_FILE, getDisplayName().c_str(), LV_LABEL_LONG_DOT);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_EVENT_BUBBLE);
    setStylePart(lvObj, "wifi.item", LV_PART_MAIN);
    setStylePart(lvObj, "wifi.item.selected", LV_STATE_CHECKED);
  }

  lv_obj_t* getLvObj() const { return lvObj; }

  // std::string updateName(bool thrown) {
  //   this->thrown = thrown;
  //   std::string displayText = getDisplayName();
  //   lv_list_set_btn_text(lvObj, displayText.c_str());
  //   // lv_list_set_btn_icon(lvObj, getImage());
  //   return displayText;
  // }

  std::string getDisplayName() const {
    return name;// + " (" + (thrown ? "Thrown" : "Closed") + ")";
  }

  // lv_img_dsc_t const* getImage() const {
  //   if(thrown) {
  //     return &turnoutclosed;
  //   }else{
  //     return &turnoutopen;
  //   }
  // }

  int getAddress() const {
    return address;
  }

 
private:
  lv_obj_t *lvObj;
  int address;
  std::string name;
};
} // namespace display
