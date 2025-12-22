#pragma once
#include "Screen.h"
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
  void unsubscribeAll();

  void button_connect_callback(lv_event_t *e);
  void button_wifi_list_callback(lv_event_t *e);
  void button_calibrate_callback(lv_event_t *e);

  void wifi_connected_callback(void *s, lv_msg_t *msg);
  void wifi_failed_callback(void *s, lv_msg_t *msg);
  void wifi_not_saved_callback(void *s, lv_msg_t *msg);

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

  static void wifi_connected_trampoline(void *s, lv_msg_t *msg) {
    auto *self = static_cast<FirstScreen *>(msg->user_data);
    if (self)
      self->wifi_connected_callback(s, msg);
  }

  static void wifi_failed_trampoline(void *s, lv_msg_t *msg) {
    auto *self = static_cast<FirstScreen *>(msg->user_data);
    if (self)
      self->wifi_failed_callback(s, msg);
  }

  static void wifi_not_saved_trampoline(void *s, lv_msg_t *msg) {
    auto *self = static_cast<FirstScreen *>(msg->user_data);
    if (self)
      self->wifi_not_saved_callback(s, msg);
  }


private:
  void *subscribe_connected;
  void *subscribe_failed;
  void *subscribe_not_saved;

  lv_obj_t *lbl_title;
  lv_obj_t *btn_connect;
  lv_obj_t *btn_wifi_scan;
  lv_obj_t *btn_cal;
  lv_obj_t *lbl_status;
  lv_obj_t *lbl_ip;

};

} // namespace display
