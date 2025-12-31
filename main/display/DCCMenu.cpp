
#include "DCCMenu.h"
#include "ConnectDCC.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "connection/wifi_control.h"
#include "TurnoutList.h"
#include "RosterList.h"
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
  lv_obj_clear_state(btn_roster, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_turnouts, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_routes, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_turntables, LV_STATE_DISABLED);
}

void DCCMenu::disableButtons() {
  ESP_LOGI(TAG, "DisableButtons");
  lv_obj_add_state(btn_roster, LV_STATE_DISABLED);
  lv_obj_add_state(btn_turnouts, LV_STATE_DISABLED);
  lv_obj_add_state(btn_routes, LV_STATE_DISABLED);
  lv_obj_add_state(btn_turntables, LV_STATE_DISABLED);
}

void DCCMenu::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  // Title Label
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

  // "Close" button
  btn_close = makeButton(lvObj_, "Close", 200, 48, LV_ALIGN_CENTER, 0, 180,"button.secondary");
  lv_obj_add_event_cb(btn_close, &DCCMenu::event_back_trampoline, LV_EVENT_CLICKED, this);

  disableButtons();

  auto wifiHandler = utilities::WifiHandler::instance();

  subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, [](void *s, lv_msg_t *msg) {
    ESP_LOGI(TAG, "Not connected to wifi");
    auto firstScreen = FirstScreen::instance();
    firstScreen->showScreen();
  },this);


  subscribe_dcc_roster_received = lv_msg_subscribe(
      MSG_DCC_ROSTER_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Roster list received message");
        // Handle roster list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        lv_label_set_text(self->lbl_status, "Roster list received.");
         lv_obj_clear_state(self->btn_roster, LV_STATE_DISABLED);
      },
      this);
  subscribe_dcc_turnout_received = lv_msg_subscribe(
      MSG_DCC_TURNOUT_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Turnout list received message");
        // Handle turnout list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        lv_label_set_text(self->lbl_status, "Turnout list received.");
         lv_obj_clear_state(self->btn_turnouts, LV_STATE_DISABLED);
      },
      this);

  subscribe_dcc_route_received = lv_msg_subscribe(
      MSG_DCC_ROUTE_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Route list received message");
        // Handle route list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        lv_label_set_text(self->lbl_status, "Route list received.");
         lv_obj_clear_state(self->btn_routes, LV_STATE_DISABLED);
      },
      this);
  subscribe_dcc_turntable_received = lv_msg_subscribe(
      MSG_DCC_TURNTABLE_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Turntable list received message");
        // Handle turntable list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        lv_label_set_text(self->lbl_status, "Turntable list received.");
         lv_obj_clear_state(self->btn_turntables, LV_STATE_DISABLED);
      },
      this);

  // Status labels under the last button
  // Show connection state and IP (IP empty when disconnected)
  lbl_status = makeLabel(lvObj_, "", LV_ALIGN_TOP_MID, 0, 40,"label.main");

  enableIfReceivedLists();

  ESP_LOGI(TAG, "DCCMenu UI created");
}

void DCCMenu::enableIfReceivedLists() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if(dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot enable buttons");
    return;
  }

  if(dccProtocol->receivedRoster()){
    ESP_LOGI(TAG, "DCC Roster list already received, enabling roster button");
    lv_obj_clear_state(btn_roster, LV_STATE_DISABLED);
  }
  if(dccProtocol->receivedTurnoutList()){
    ESP_LOGI(TAG, "DCC Turnout list already received, enabling turnouts button");
    lv_obj_clear_state(btn_turnouts, LV_STATE_DISABLED);
  }
  if( dccProtocol->receivedRouteList()){
    ESP_LOGI(TAG, "DCC Route list already received, enabling routes button");
    lv_obj_clear_state(btn_routes, LV_STATE_DISABLED);
  }
  if(dccProtocol->receivedTurntableList()) {
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
  lv_obj_clean(lvObj_);
}

void DCCMenu::button_roster_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Roster button clicked!");
    unsubscribeAll();

    auto rosterListScreen = RosterListScreen::instance();
    rosterListScreen->showScreen(shared_from_this());
  }
}

void DCCMenu::button_turnouts_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turnout button clicked!");
    unsubscribeAll();

    auto turnoutListScreen = TurnoutListScreen::instance();
    turnoutListScreen->showScreen(shared_from_this());

  }
}

void DCCMenu::button_routes_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Routes button clicked!");
    unsubscribeAll();
  }
}

void DCCMenu::button_turntables_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turntables button clicked!");

    unsubscribeAll();
  }
}

void DCCMenu::button_back_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Close button clicked!");
    unsubscribeAll();
    auto wifiControl = utilities::WifiControl::instance();
    wifiControl->disconnect();
    auto firstScreen = FirstScreen::instance();
    firstScreen->showScreen();
  }
}

} // namespace display
