#pragma once

#include "Screen.h"
#include <memory>

namespace display {

class DCCMenu : public Screen, public std::enable_shared_from_this<DCCMenu> {
public:
  static std::shared_ptr<DCCMenu> instance() {
    static std::shared_ptr<DCCMenu> s;
    if (!s)
      s.reset(new DCCMenu());
    return s;
  }

  ~DCCMenu() override = default; 
  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  DCCMenu(const DCCMenu &) = delete;
  DCCMenu &operator=(const DCCMenu &) = delete; 
  void enableButtons(bool enableConnect);
  void disableButtons();
  void unsubscribeAll();

  lv_obj_t *lbl_status;

  void setConnectedServer(std::string ip, int port, std::string dccname);

  void button_roster_callback(lv_event_t *e);
  void button_points_callback(lv_event_t *e);
  void button_routes_callback(lv_event_t *e);
  void button_turntables_callback(lv_event_t *e);
  void button_back_callback(lv_event_t *e);
protected:
  DCCMenu() = default;  

  static void event_roster_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_roster_callback(e);
  }

  static void event_points_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_points_callback(e);
  }

  static void event_routes_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_routes_callback(e);
  }


  static void event_turntables_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_turntables_callback(e);
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

private:
  void *subscribe_failed;
  void *subscribe_not_saved;

  void *subscribe_dcc_roster_received;
  void *subscribe_dcc_turnout_received;
  void *subscribe_dcc_route_received;
  void *subscribe_dcc_turntable_received;

  std::string ip;
  int port;
  std::string dccname;

  lv_obj_t *lbl_title;
  lv_obj_t *btn_roster;
  lv_obj_t *btn_points;
  lv_obj_t *btn_routes;
  lv_obj_t *btn_turntables;
  lv_obj_t *btn_close;
};

} // namespace display

