#pragma once
#include "Screen.h"
#include "ui/lv_msg.h"
#include <memory>

namespace display {

class FirstScreen : public Screen, public std::enable_shared_from_this<FirstScreen> {
public:
  static std::shared_ptr<FirstScreen> instance() {
    static std::shared_ptr<FirstScreen> s;
    if (!s)
      s.reset(new FirstScreen());
    return s;
  }

  ~FirstScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  FirstScreen(const FirstScreen &) = delete;
  FirstScreen &operator=(const FirstScreen &) = delete;

  void enableButtons(bool enableConnect);
  void disableButtons();

  void button_connect_callback(lv_event_t *e);
  void button_wifi_list_callback(lv_event_t *e);
  void button_calibrate_callback(lv_event_t *e);

  void wifi_connected_callback(lv_msg_t *msg);
  void wifi_not_saved_callback(lv_msg_t *msg);

protected:
  FirstScreen() = default;

  static void event_connect_trampoline(lv_event_t *e) {
    auto *self = static_cast<FirstScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_connect_callback(e);
  }

  static void event_wifi_list_trampoline(lv_event_t *e) {
    auto *self = static_cast<FirstScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_wifi_list_callback(e);
  }

  static void event_calibrate_trampoline(lv_event_t *e) {
    auto *self = static_cast<FirstScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_calibrate_callback(e);
  }

  static void wifi_connected_trampoline(lv_msg_t *msg) {
    auto *self = static_cast<FirstScreen *>(lv_msg_get_user_data(msg));
    if (self)
      self->wifi_connected_callback(msg);
  }

  static void wifi_not_saved_trampoline(lv_msg_t *msg) {
    auto *self = static_cast<FirstScreen *>(lv_msg_get_user_data(msg));
    if (self)
      self->wifi_not_saved_callback(msg);
  }

private:
  bool isCleanedUp = false;
  lv_msg_sub_dsc_t *subscribe_connected = nullptr;
  lv_msg_sub_dsc_t *subscribe_not_saved = nullptr;

  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *btn_connect = nullptr;
  lv_obj_t *btn_wifi_scan = nullptr;
  lv_obj_t *btn_cal = nullptr;
  lv_obj_t *lbl_status = nullptr;
  lv_obj_t *lbl_ip = nullptr;
};

} // namespace display
