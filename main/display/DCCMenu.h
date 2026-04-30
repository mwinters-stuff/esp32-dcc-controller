#pragma once

#include "RotaryListScreenBase.h"
#include <DCCEXProtocol.h>
#include <memory>

namespace display {

class DCCMenu : public RotaryListScreenBase, public std::enable_shared_from_this<DCCMenu> {
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
  void button_turnouts_callback(lv_event_t *e);
  void button_routes_callback(lv_event_t *e);
  void button_turntables_callback(lv_event_t *e);
  void button_refresh_callback(lv_event_t *e);
  void button_track_power_callback(lv_event_t *e);
  void button_back_callback(lv_event_t *e);

protected:
  DCCMenu() = default;

  static void event_roster_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_roster_callback(e);
  }

  static void event_turnouts_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_turnouts_callback(e);
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

  static void event_refresh_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_refresh_callback(e);
  }

  static void event_track_power_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_track_power_callback(e);
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<DCCMenu *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  void enableIfReceivedLists();
  void setStatusText(const char *text);
  void ensureStatusHideTimer();

private:
  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override;
  void rotaryActivateFocused() override;

  void moveFocus(int direction);
  void updateFocusedState();

  int focusedIndex = -1;
  lv_msg_sub_dsc_t *subscribe_dcc_roster_received = nullptr;
  lv_msg_sub_dsc_t *subscribe_dcc_turnout_received = nullptr;
  lv_msg_sub_dsc_t *subscribe_dcc_route_received = nullptr;
  lv_msg_sub_dsc_t *subscribe_dcc_turntable_received = nullptr;
  lv_msg_sub_dsc_t *subscribe_dcc_track_power = nullptr;

  std::string ip;
  int port;
  std::string dccname;

  bool isCleanedUp = false;
  lv_timer_t *statusHideTimer = nullptr;
  std::string lastStatusText;
  lv_obj_t *lbl_title;
  lv_obj_t *btn_roster;
  lv_obj_t *btn_turnouts;
  lv_obj_t *btn_routes;
  lv_obj_t *btn_turntables;
  lv_obj_t *btn_refresh;
  lv_obj_t *btn_track_power;
  lv_obj_t *btn_close;
  DCCExController::TrackPower trackPowerState = DCCExController::PowerOff;
};

} // namespace display
