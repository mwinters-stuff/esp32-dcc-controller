#pragma once
#include "LvglWrapper.h"
#include <DCCEXLoco.h>
#include <esp_log.h>
#include <functional>
#include <lvgl.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace display {

class RosterListItem {
public:
  using SelectCallback = std::function<void(int address)>;
  using ThrottleCallback = std::function<void(int address, int speed, Direction direction)>;
  using StopCallback = std::function<void(int address)>;
  using FunctionCallback = std::function<void(int address, int function, bool on)>;

  lv_obj_t *parentObj;
  size_t index;

  RosterListItem(lv_obj_t *parent, size_t index, Loco *loco, SelectCallback onSelect, ThrottleCallback onThrottle,
                 StopCallback onStop, FunctionCallback onFunction)
      : parentObj(parent), index(index), selectCallback_(std::move(onSelect)), throttleCallback_(std::move(onThrottle)),
        stopCallback_(std::move(onStop)), functionCallback_(std::move(onFunction)) {
    if (loco) {
      address = loco->getAddress();
      const char *locoName = loco->getName();
      if (locoName != nullptr && locoName[0] != '\0') {
        name = locoName;
      } else {
        name = "Loco " + std::to_string(address);
      }
      speed_ = loco->getSpeed();
      direction_ = loco->getDirection();
      functionMap_ = loco->getFunctionStates();
    }

    lvObj = lv_obj_create(parent);
    lv_obj_set_width(lvObj, lv_pct(100));
    lv_obj_set_height(lvObj, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(lvObj, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(lvObj, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_left(lvObj, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_right(lvObj, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(lvObj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(lvObj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(lvObj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lvObj, &RosterListItem::event_row_click_trampoline, LV_EVENT_CLICKED, this);

    headerRow_ = lv_obj_create(lvObj);
    makeFlatContainer_(headerRow_);
    lv_obj_set_width(headerRow_, lv_pct(100));
    lv_obj_set_flex_flow(headerRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerRow_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    nameLabel_ = lv_label_create(headerRow_);
    lv_label_set_text(nameLabel_, getDisplayName().c_str());
    lv_obj_set_flex_grow(nameLabel_, 1);

    directionLabel_ = lv_label_create(headerRow_);

    speedValueLabel_ = lv_label_create(headerRow_);

    speedBar_ = lv_bar_create(lvObj);
    lv_obj_set_width(speedBar_, lv_pct(100));
    lv_bar_set_range(speedBar_, 0, 126);

    expandedPanel_ = lv_obj_create(lvObj);
    makeFlatContainer_(expandedPanel_);
    lv_obj_set_width(expandedPanel_, lv_pct(100));
    lv_obj_set_flex_flow(expandedPanel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(expandedPanel_, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(expandedPanel_, 6, LV_PART_MAIN);

    controlsRow1_ = lv_obj_create(expandedPanel_);
    makeFlatContainer_(controlsRow1_);
    lv_obj_set_width(controlsRow1_, lv_pct(100));
    lv_obj_set_flex_flow(controlsRow1_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(controlsRow1_, 6, LV_PART_MAIN);

    btnReverse_ = lv_btn_create(controlsRow1_);
    lv_obj_add_event_cb(btnReverse_, &RosterListItem::event_reverse_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblReverse = lv_label_create(btnReverse_);
    lv_label_set_text(lblReverse, LV_SYMBOL_LEFT);

    btnBrake_ = lv_btn_create(controlsRow1_);
    lv_obj_add_event_cb(btnBrake_, &RosterListItem::event_brake_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblBrake = lv_label_create(btnBrake_);
    lv_label_set_text(lblBrake, "Brake");

    btnStop_ = lv_btn_create(controlsRow1_);
    lv_obj_add_event_cb(btnStop_, &RosterListItem::event_stop_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblStop = lv_label_create(btnStop_);
    lv_label_set_text(lblStop, "Stop");

    btnForward_ = lv_btn_create(controlsRow1_);
    lv_obj_add_event_cb(btnForward_, &RosterListItem::event_forward_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblForward = lv_label_create(btnForward_);
    lv_label_set_text(lblForward, LV_SYMBOL_RIGHT);

    controlsRow2_ = lv_obj_create(expandedPanel_);
    makeFlatContainer_(controlsRow2_);
    lv_obj_set_width(controlsRow2_, lv_pct(100));
    lv_obj_set_flex_flow(controlsRow2_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controlsRow2_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btnSpeedDown_ = lv_btn_create(controlsRow2_);
    lv_obj_add_event_cb(btnSpeedDown_, &RosterListItem::event_speed_down_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblSpeedDown = lv_label_create(btnSpeedDown_);
    lv_label_set_text(lblSpeedDown, "-");

    expandedSpeedLabel_ = lv_label_create(controlsRow2_);

    btnSpeedUp_ = lv_btn_create(controlsRow2_);
    lv_obj_add_event_cb(btnSpeedUp_, &RosterListItem::event_speed_up_trampoline, LV_EVENT_CLICKED, this);
    lv_obj_t *lblSpeedUp = lv_label_create(btnSpeedUp_);
    lv_label_set_text(lblSpeedUp, "+");

    functionsLabel_ = lv_label_create(expandedPanel_);
    lv_label_set_text(functionsLabel_, "Functions");

    functionsContainer_ = lv_obj_create(expandedPanel_);
    makeFlatContainer_(functionsContainer_);
    lv_obj_set_width(functionsContainer_, lv_pct(100));
    lv_obj_set_flex_flow(functionsContainer_, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(functionsContainer_, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(functionsContainer_, 6, LV_PART_MAIN);

    loco_ = loco;
    functionsBuilt_ = false;

    setExpanded(false);
    updateState(speed_, direction_, functionMap_);
  }

  lv_obj_t *getLvObj() const { return lvObj; }

  std::string getDisplayName() const { return name; }

  int getAddress() const { return address; }

  void setExpanded(bool expanded) {
    expanded_ = expanded;
    if (expanded_) {
      lv_obj_clear_flag(expandedPanel_, LV_OBJ_FLAG_HIDDEN);
      // Lazy-load function buttons on first expansion
      if (!functionsBuilt_ && loco_) {
        buildFunctionButtons_(loco_);
        functionsBuilt_ = true;
      }
    } else {
      lv_obj_add_flag(expandedPanel_, LV_OBJ_FLAG_HIDDEN);
    }
  }

  bool isExpanded() const { return expanded_; }

  void updateState(int speed, Direction direction, int functionMap) {
    speed_ = speed;
    if (speed_ < 0) {
      speed_ = 0;
    } else if (speed_ > 126) {
      speed_ = 126;
    }
    direction_ = direction;
    functionMap_ = functionMap;

    lv_bar_set_value(speedBar_, speed_, LV_ANIM_OFF);

    char speedText[32];
    std::snprintf(speedText, sizeof(speedText), "S:%d", speed_);
    lv_label_set_text(speedValueLabel_, speedText);
    lv_label_set_text(expandedSpeedLabel_, speedText);
    lv_label_set_text(directionLabel_, direction_ == Forward ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT);

    for (const auto &entry : functionButtons_) {
      const bool on = (functionMap_ & (1 << entry.first)) != 0;
      if (on) {
        lv_obj_add_state(entry.second, LV_STATE_CHECKED);
      } else {
        lv_obj_clear_state(entry.second, LV_STATE_CHECKED);
      }
    }
  }

private:
  lv_obj_t *lvObj;
  lv_obj_t *headerRow_ = nullptr;
  lv_obj_t *nameLabel_ = nullptr;
  lv_obj_t *directionLabel_ = nullptr;
  lv_obj_t *speedValueLabel_ = nullptr;
  lv_obj_t *speedBar_ = nullptr;

  lv_obj_t *expandedPanel_ = nullptr;
  lv_obj_t *controlsRow1_ = nullptr;
  lv_obj_t *controlsRow2_ = nullptr;
  lv_obj_t *btnReverse_ = nullptr;
  lv_obj_t *btnBrake_ = nullptr;
  lv_obj_t *btnStop_ = nullptr;
  lv_obj_t *btnForward_ = nullptr;
  lv_obj_t *btnSpeedDown_ = nullptr;
  lv_obj_t *btnSpeedUp_ = nullptr;
  lv_obj_t *expandedSpeedLabel_ = nullptr;
  lv_obj_t *functionsLabel_ = nullptr;
  lv_obj_t *functionsContainer_ = nullptr;

  std::unordered_map<int, lv_obj_t *> functionButtons_;

  int address;
  std::string name;
  int speed_ = 0;
  Direction direction_ = Forward;
  int functionMap_ = 0;
  bool expanded_ = false;
  bool functionsBuilt_ = false;
  Loco *loco_ = nullptr;

  SelectCallback selectCallback_;
  ThrottleCallback throttleCallback_;
  StopCallback stopCallback_;
  FunctionCallback functionCallback_;

  static void makeFlatContainer_(lv_obj_t *obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
  }

  void buildFunctionButtons_(Loco *loco) {
    for (int fn = 0; fn <= 27; ++fn) {
      const char *functionName = loco->getFunctionName(fn);
      if (functionName == nullptr || functionName[0] == '\0') {
        continue;
      }

      lv_obj_t *btn = lv_btn_create(functionsContainer_);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      lv_obj_add_event_cb(btn, &RosterListItem::event_function_trampoline, LV_EVENT_CLICKED, this);

      char btnLabel[64];
      std::snprintf(btnLabel, sizeof(btnLabel), "F%d %s", fn, functionName);
      lv_obj_t *lbl = lv_label_create(btn);
      lv_label_set_text(lbl, btnLabel);

      functionButtons_[fn] = btn;
    }
  }

  int functionFromButton_(lv_obj_t *btn) const {
    for (const auto &entry : functionButtons_) {
      if (entry.second == btn) {
        return entry.first;
      }
    }
    return -1;
  }

  static void event_row_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleRowClick_(e);
    }
  }

  static void event_reverse_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleReverseClick_(e);
    }
  }

  static void event_brake_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleBrakeClick_(e);
    }
  }

  static void event_stop_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleStopClick_(e);
    }
  }

  static void event_forward_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleForwardClick_(e);
    }
  }

  static void event_speed_down_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleSpeedDeltaClick_(e, -5);
    }
  }

  static void event_speed_up_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleSpeedDeltaClick_(e, 5);
    }
  }

  static void event_function_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListItem *>(lv_event_get_user_data(e));
    if (self) {
      self->handleFunctionClick_(e);
    }
  }

  void handleRowClick_(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
      return;
    }

    if (selectCallback_) {
      selectCallback_(address);
    }
  }

  void handleReverseClick_(lv_event_t *e) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !throttleCallback_) {
      return;
    }
    throttleCallback_(address, speed_, Reverse);
  }

  void handleBrakeClick_(lv_event_t *e) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !throttleCallback_) {
      return;
    }
    throttleCallback_(address, 0, direction_);
  }

  void handleStopClick_(lv_event_t *e) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !stopCallback_) {
      return;
    }
    stopCallback_(address);
  }

  void handleForwardClick_(lv_event_t *e) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !throttleCallback_) {
      return;
    }
    throttleCallback_(address, speed_, Forward);
  }

  void handleSpeedDeltaClick_(lv_event_t *e, int delta) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !throttleCallback_) {
      return;
    }

    int newSpeed = speed_ + delta;
    if (newSpeed < 0) {
      newSpeed = 0;
    } else if (newSpeed > 126) {
      newSpeed = 126;
    }
    throttleCallback_(address, newSpeed, direction_);
  }

  void handleFunctionClick_(lv_event_t *e) {
    lv_event_stop_bubbling(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !functionCallback_) {
      return;
    }

    lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    const int function = functionFromButton_(btn);
    if (function < 0) {
      return;
    }

    const bool on = lv_obj_has_state(btn, LV_STATE_CHECKED);
    functionCallback_(address, function, on);
  }
};
} // namespace display
