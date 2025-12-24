#pragma once
#include "Screen.h"
#include <lvgl.h>
#include <memory>
#include <string>

namespace display {

class WaitingScreen : public Screen {
public:
  // static std::shared_ptr<WaitingScreen> instance() {
  //   static std::shared_ptr<WaitingScreen> s;
  //   if (!s)
  //     s.reset(new WaitingScreen());
  //   return s;
  // }

  // WaitingScreen(const WaitingScreen &) = delete;
  // WaitingScreen &operator=(const WaitingScreen &) = delete;
  WaitingScreen() = default;
  ~WaitingScreen() override {
    unsubscribeAll();
  };

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void setLabel(const std::string &text);
  void setSubLabel(const std::string &text);
  void unsubscribeAll();
private:
  std::string message;
  std::string subMessage;
  lv_obj_t *label;
  lv_obj_t *sub_label;
  lv_obj_t *spinner;
  void *msg_subscribe_success = nullptr;
  void *msg_subscribe_failed = nullptr;

protected:
  // WaitingScreen() = default;
};

} // namespace display