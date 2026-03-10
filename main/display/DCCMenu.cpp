
#include "DCCMenu.h"
#include "ConnectDCC.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "RosterList.h"
#include "TurnTableList.h"
#include "TurnoutList.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "DCC_MENU_SCREEN";

void DCCMenu::setConnectedServer(std::string ip, int port, std::string dccname) {
  this->ip = ip;
  this->port = port;
  this->dccname = dccname;
}

void DCCMenu::enableButtons(bool enableConnect) {
  ESP_LOGI(TAG, "EnableButtons");
  if (btn_roster)
    lv_obj_clear_state(btn_roster, LV_STATE_DISABLED);
  if (btn_turnouts)
    lv_obj_clear_state(btn_turnouts, LV_STATE_DISABLED);
  if (btn_routes)
    lv_obj_clear_state(btn_routes, LV_STATE_DISABLED);
  if (btn_turntables)
    lv_obj_clear_state(btn_turntables, LV_STATE_DISABLED);
}

void DCCMenu::disableButtons() {
  ESP_LOGI(TAG, "DisableButtons");
  if (btn_roster)
    lv_obj_add_state(btn_roster, LV_STATE_DISABLED);
  if (btn_turnouts)
    lv_obj_add_state(btn_turnouts, LV_STATE_DISABLED);
  if (btn_routes)
    lv_obj_add_state(btn_routes, LV_STATE_DISABLED);
  if (btn_turntables)
    lv_obj_add_state(btn_turntables, LV_STATE_DISABLED);
}

void DCCMenu::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  // Title Label
  isCleanedUp = false;
  lv_obj_clean(lv_screen_active());
  lvObj_ = lv_screen_active();

  lbl_title = makeLabel(lvObj_, dccname.c_str(), LV_ALIGN_TOP_MID, 0, 10, "label.title", &lv_font_montserrat_30);

  // "Roster" button
  btn_roster = makeButton(lvObj_, "Roster", 200, 48, LV_ALIGN_CENTER, 0, -120, "button.primary");
  lv_obj_add_event_cb(btn_roster, &DCCMenu::event_roster_trampoline, LV_EVENT_CLICKED, this);
  // "Turnouts" button

  btn_turnouts = makeButton(lvObj_, "Turnouts", 200, 48, LV_ALIGN_CENTER, 0, -60, "button.primary");
  lv_obj_add_event_cb(btn_turnouts, &DCCMenu::event_turnouts_trampoline, LV_EVENT_CLICKED, this);

  // "Routes" button
  btn_routes = makeButton(lvObj_, "Routes", 200, 48, LV_ALIGN_CENTER, 0, 0, "button.primary");
  lv_obj_add_event_cb(btn_routes, &DCCMenu::event_routes_trampoline, LV_EVENT_CLICKED, this);

  // "Turntables" button
  btn_turntables = makeButton(lvObj_, "Turntables", 200, 48, LV_ALIGN_CENTER, 0, 60, "button.primary");
  lv_obj_add_event_cb(btn_turntables, &DCCMenu::event_turntables_trampoline, LV_EVENT_CLICKED, this);

  // Icon-only refresh button
  btn_refresh = makeButton(lvObj_, LV_SYMBOL_REFRESH, 48, 48, LV_ALIGN_CENTER, 0, 120, "button.secondary");
  lv_obj_add_event_cb(btn_refresh, &DCCMenu::event_refresh_trampoline, LV_EVENT_CLICKED, this);

  // "Close" button
  btn_close = makeButton(lvObj_, "Close", 200, 48, LV_ALIGN_CENTER, 0, 180, "button.secondary");
  lv_obj_add_event_cb(btn_close, &DCCMenu::event_back_trampoline, LV_EVENT_CLICKED, this);

  disableButtons();

  auto wifiHandler = utilities::WifiHandler::instance();

  subscribe_failed = lv_msg_subscribe(
      MSG_WIFI_FAILED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "Not connected to wifi");
        self->cleanUp();
        auto firstScreen = FirstScreen::instance();
        firstScreen->showScreen();
      },
      this);

  subscribe_dcc_roster_received = lv_msg_subscribe(
      MSG_DCC_ROSTER_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "DCC Roster list received message");
        if (self->lbl_status)
          lv_label_set_text(self->lbl_status, "Roster list received.");
        if (self->btn_roster)
          lv_obj_clear_state(self->btn_roster, LV_STATE_DISABLED);
      },
      this);
  subscribe_dcc_turnout_received = lv_msg_subscribe(
      MSG_DCC_TURNOUT_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "DCC Turnout list received message");
        if (self->lbl_status)
          lv_label_set_text(self->lbl_status, "Turnout list received.");
        if (self->btn_turnouts)
          lv_obj_clear_state(self->btn_turnouts, LV_STATE_DISABLED);
      },
      this);

  subscribe_dcc_route_received = lv_msg_subscribe(
      MSG_DCC_ROUTE_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "DCC Route list received message");
        if (self->lbl_status)
          lv_label_set_text(self->lbl_status, "Route list received.");
        if (self->btn_routes)
          lv_obj_clear_state(self->btn_routes, LV_STATE_DISABLED);
      },
      this);
  subscribe_dcc_turntable_received = lv_msg_subscribe(
      MSG_DCC_TURNTABLE_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "DCC Turntable list received message");
        if (self->lbl_status)
          lv_label_set_text(self->lbl_status, "Turntable list received.");
        if (self->btn_turntables)
          lv_obj_clear_state(self->btn_turntables, LV_STATE_DISABLED);
      },
      this);

  // Status labels under the last button
  // Show connection state and IP (IP empty when disconnected)
  lbl_status = makeLabel(lvObj_, "", LV_ALIGN_TOP_MID, 0, 40, "label.main");

  enableIfReceivedLists();

  ESP_LOGI(TAG, "DCCMenu UI created");
}

void DCCMenu::enableIfReceivedLists() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot enable buttons");
    return;
  }

  if (dccProtocol->receivedRoster() && dccProtocol->getRosterCount() > 0) {
    ESP_LOGI(TAG, "DCC Roster list already received, enabling roster button");
    lv_obj_clear_state(btn_roster, LV_STATE_DISABLED);
  }
  if (dccProtocol->receivedTurnoutList() && dccProtocol->getTurnoutCount() > 0) {
    ESP_LOGI(TAG, "DCC Turnout list already received, enabling turnouts button");
    lv_obj_clear_state(btn_turnouts, LV_STATE_DISABLED);
  }
  if (dccProtocol->receivedRouteList() && dccProtocol->getRouteCount() > 0) {
    ESP_LOGI(TAG, "DCC Route list already received, enabling routes button");
    lv_obj_clear_state(btn_routes, LV_STATE_DISABLED);
  }
  if (dccProtocol->receivedTurntableList() && dccProtocol->getTurntableCount() > 0) {
    ESP_LOGI(TAG, "DCC Turntable list already received, enabling turntables button");
    lv_obj_clear_state(btn_turntables, LV_STATE_DISABLED);
  }
}

void DCCMenu::unsubscribeAll() {
  if (subscribe_failed != nullptr) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
  if (subscribe_not_saved != nullptr) {
    lv_msg_unsubscribe(subscribe_not_saved);
    subscribe_not_saved = nullptr;
  }

  if (subscribe_dcc_roster_received != nullptr) {
    lv_msg_unsubscribe(subscribe_dcc_roster_received);
    subscribe_dcc_roster_received = nullptr;
  }
  if (subscribe_dcc_turnout_received != nullptr) {
    lv_msg_unsubscribe(subscribe_dcc_turnout_received);
    subscribe_dcc_turnout_received = nullptr;
  }
  if (subscribe_dcc_route_received != nullptr) {
    lv_msg_unsubscribe(subscribe_dcc_route_received);
    subscribe_dcc_route_received = nullptr;
  }
  if (subscribe_dcc_turntable_received != nullptr) {
    lv_msg_unsubscribe(subscribe_dcc_turntable_received);
    subscribe_dcc_turntable_received = nullptr;
  }
}

void DCCMenu::cleanUp() {
  ESP_LOGI(TAG, "DCCMenu cleaned up");
  isCleanedUp = true;
  unsubscribeAll();
  lv_obj_clean(lvObj_);
  btn_roster = nullptr;
  btn_turnouts = nullptr;
  btn_routes = nullptr;
  btn_turntables = nullptr;
  btn_refresh = nullptr;
  btn_close = nullptr;
  lbl_title = nullptr;
  lbl_status = nullptr;
}

void DCCMenu::button_roster_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Roster button clicked!");
    cleanUp();

    auto rosterListScreen = RosterListScreen::instance();
    rosterListScreen->showScreen(shared_from_this());
  }
}

void DCCMenu::button_turnouts_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turnout button clicked!");
    cleanUp();

    auto turnoutListScreen = TurnoutListScreen::instance();
    turnoutListScreen->showScreen(shared_from_this());
  }
}

void DCCMenu::button_routes_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Routes button clicked!");
    cleanUp();
  }
}

void DCCMenu::button_turntables_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turntables button clicked!");

    cleanUp();

    auto turntableListScreen = TurnTableListScreen::instance();
    turntableListScreen->showScreen(shared_from_this());
  }
}

void DCCMenu::button_refresh_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Refresh button clicked!");
    disableButtons();
    auto wifiControl = utilities::WifiControl::instance();
    auto dccProtocol = wifiControl->dccProtocol();
    if (dccProtocol == nullptr) {
      ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh lists");
      return;
    }
    dccProtocol->refreshAllLists();
    if (lbl_status)
      lv_label_set_text(lbl_status, "Refreshing lists...");
  }
}

void DCCMenu::button_back_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Close button clicked!");
    auto wifiControl = utilities::WifiControl::instance();
    wifiControl->disconnect();

    cleanUp();

    auto firstScreen = FirstScreen::instance();
    firstScreen->showScreen();
  }
}

} // namespace display
