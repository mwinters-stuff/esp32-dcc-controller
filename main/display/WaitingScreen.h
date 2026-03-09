#pragma once
#include "Screen.h"
#include <esp_timer.h>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class WaitingScreen : public Screen {
public:
  WaitingScreen() = default;
  ~WaitingScreen() override { unsubscribeAll(); };

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void setLabel(const std::string &text);
  void setSubLabel(const std::string &text);
  void unsubscribeAll();

private:
  bool isCleanedUp = false;
  std::string message;
  std::string subMessage;
  lv_obj_t *label = nullptr;
  lv_obj_t *sub_label = nullptr;
  lv_obj_t *spinner = nullptr;
  lv_msg_sub_dsc_t *msg_subscribe_success = nullptr;
  lv_msg_sub_dsc_t *msg_subscribe_failed = nullptr;
  esp_timer_handle_t failure_timer = nullptr;

protected:
  // WaitingScreen() = default;
};

} // namespace display