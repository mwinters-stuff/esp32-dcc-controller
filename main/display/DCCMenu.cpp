
/**
 * @file DCCMenu.cpp
 * @brief Main DCC operations menu shown after a successful server connection.
 *
 * Displays navigation buttons for Roster, Turnouts, Routes and Turntables,
 * along with refresh and track-power toggle controls. Buttons are enabled as
 * the DCC server delivers each list. Closing this screen disconnects from the
 * server and returns to the home screen.
 */
#include "DCCMenu.h"
#include "ConnectDCC.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "RosterList.h"
#include "RouteList.h"
#include "TurnoutList.h"
#include "TurntableList.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "DCC_MENU_SCREEN";

namespace {
constexpr uint32_t STATUS_HIDE_DELAY_MS = 20000;
}

// Stores the IP, port and display name of the server we just connected to.
// Called by ConnectDCCScreen before showing this screen.
void DCCMenu::setConnectedServer(std::string ip, int port, std::string dccname) {
  this->ip = ip;
  this->port = port;
  this->dccname = dccname;
}

// Ensures a one-shot timer exists that hides the status label when it fires.
void DCCMenu::ensureStatusHideTimer() {
  if (statusHideTimer != nullptr) {
    return;
  }

  statusHideTimer = lv_timer_create(
      [](lv_timer_t *timer) {
        auto *self = static_cast<DCCMenu *>(lv_timer_get_user_data(timer));
        if (!self || self->isCleanedUp || !self->lbl_status) {
          return;
        }
        lv_obj_add_flag(self->lbl_status, LV_OBJ_FLAG_HIDDEN);
      },
      STATUS_HIDE_DELAY_MS, this);
}

// Updates the status text, makes it visible and hides it again after 20s if
// the text is not updated.
void DCCMenu::setStatusText(const char *text) {
  if (!lbl_status || text == nullptr) {
    return;
  }

  std::string newText = text;
  if (newText == lastStatusText) {
    return;
  }

  lastStatusText = newText;
  lv_label_set_text(lbl_status, text);
  lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);

  ensureStatusHideTimer();
  if (statusHideTimer) {
    lv_timer_reset(statusHideTimer);
  }
}

// Clears the disabled state on all four category buttons (Roster, Turnouts,
// Routes, Turntables) so the user can navigate into them.
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

// Adds the disabled state to all four category buttons. Called during screen
// setup and when the list refresh is in progress.
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

// Builds and renders the DCC menu UI, creates all buttons and subscribes to
// DCC list/power messages. Buttons that have already received their data lists
// are enabled immediately via enableIfReceivedLists().
void DCCMenu::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  // Title Label
  isCleanedUp = false;
  lastStatusText.clear();
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
  btn_refresh = makeButton(lvObj_, LV_SYMBOL_REFRESH, 48, 48, LV_ALIGN_CENTER, -30, 120, "button.secondary");
  lv_obj_add_event_cb(btn_refresh, &DCCMenu::event_refresh_trampoline, LV_EVENT_CLICKED, this);

  // Icon-only track power toggle button
  btn_track_power = makeButton(lvObj_, LV_SYMBOL_POWER, 48, 48, LV_ALIGN_CENTER, 30, 120, "button.secondary");
  lv_obj_add_event_cb(btn_track_power, &DCCMenu::event_track_power_trampoline, LV_EVENT_CLICKED, this);

  // "Close" button
  btn_close = makeButton(lvObj_, "Close", 200, 48, LV_ALIGN_CENTER, 0, 180, "button.secondary");
  lv_obj_add_event_cb(btn_close, &DCCMenu::event_back_trampoline, LV_EVENT_CLICKED, this);

  disableButtons();

  subscribe_dcc_roster_received = lv_msg_subscribe(
      MSG_DCC_ROSTER_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "DCC Roster list received message");
        self->setStatusText("Roster list received.");
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
        self->setStatusText("Turnout list received.");
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
        self->setStatusText("Route list received.");
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
        self->setStatusText("Turntable list received.");
        if (self->btn_turntables)
          lv_obj_clear_state(self->btn_turntables, LV_STATE_DISABLED);
      },
      this);

  subscribe_dcc_track_power = lv_msg_subscribe(
      MSG_DCC_TRACK_POWER_CHANGED,
      [](lv_msg_t *msg) {
        DCCMenu *self = static_cast<DCCMenu *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        const auto *statePayload = static_cast<const uint8_t *>(lv_msg_get_payload(msg));
        if (!statePayload)
          return;

        self->trackPowerState = static_cast<DCCExController::TrackPower>(*statePayload);
        if (!self->btn_track_power)
          return;

        if (self->trackPowerState == DCCExController::PowerOn) {
          lv_obj_set_style_text_color(self->btn_track_power, lv_color_hex(0x2E7D32), LV_PART_MAIN);
        } else if (self->trackPowerState == DCCExController::PowerOff) {
          lv_obj_set_style_text_color(self->btn_track_power, lv_color_hex(0xC62828), LV_PART_MAIN);
        } else {
          lv_obj_set_style_text_color(self->btn_track_power, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }
      },
      this);

  // Status labels under the last button
  // Show connection state and IP (IP empty when disconnected)
  lbl_status = makeLabel(lvObj_, "", LV_ALIGN_TOP_MID, 0, 40, "label.main");
  ensureStatusHideTimer();

  enableIfReceivedLists();

  ESP_LOGI(TAG, "DCCMenu UI created");
}

// Checks whether the DCC protocol has already delivered each list and enables
// the corresponding button immediately, covering the case where lists arrived
// before this screen was shown (e.g. returning from a sub-screen).
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

// Removes all lv_msg subscriptions registered during show(). Must be called
// before the screen widgets are destroyed to prevent stale callbacks.
void DCCMenu::unsubscribeAll() {
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
  if (subscribe_dcc_track_power != nullptr) {
    lv_msg_unsubscribe(subscribe_dcc_track_power);
    subscribe_dcc_track_power = nullptr;
  }
}

// Tears down the screen: unsubscribes all messages, clears LVGL objects and
// nulls every widget pointer. Sets isCleanedUp so in-flight callbacks no-op.
void DCCMenu::cleanUp() {
  if (isCleanedUp) {
    return;
  }

  ESP_LOGI(TAG, "DCCMenu cleaned up");
  isCleanedUp = true;

  if (statusHideTimer != nullptr) {
    lv_timer_delete(statusHideTimer);
    statusHideTimer = nullptr;
  }

  unsubscribeAll();
  lv_obj_clean(lvObj_);
  btn_roster = nullptr;
  btn_turnouts = nullptr;
  btn_routes = nullptr;
  btn_turntables = nullptr;
  btn_refresh = nullptr;
  btn_track_power = nullptr;
  btn_close = nullptr;
  lbl_title = nullptr;
  lbl_status = nullptr;
}

// Navigates to the Roster list screen, passing this screen as the parent so
// the back button can return here.
void DCCMenu::button_roster_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Roster button clicked!");
    cleanUp();

    auto rosterListScreen = RosterListScreen::instance();
    rosterListScreen->showScreen(shared_from_this());
  }
}

// Navigates to the Turnout list screen.
void DCCMenu::button_turnouts_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turnout button clicked!");
    cleanUp();

    auto turnoutListScreen = TurnoutListScreen::instance();
    turnoutListScreen->showScreen(shared_from_this());
  }
}

// Navigates to the Route list screen.
void DCCMenu::button_routes_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Routes button clicked!");
    cleanUp();

    auto routeListScreen = RouteListScreen::instance();
    routeListScreen->showScreen(shared_from_this());
  }
}

// Navigates to the Turntable list screen.
void DCCMenu::button_turntables_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "Turntables button clicked!");

    cleanUp();

    auto turntableListScreen = TurntableListScreen::instance();
    turntableListScreen->showScreen(shared_from_this());
  }
}

// Re-requests all DCC lists from the server, disabling the category buttons
// until the responses arrive.
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
    setStatusText("Refreshing lists...");
  }
}

// Toggles main track power on or off based on the current trackPowerState.
void DCCMenu::button_track_power_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    auto wifiControl = utilities::WifiControl::instance();
    auto dccProtocol = wifiControl->dccProtocol();
    if (dccProtocol == nullptr) {
      ESP_LOGW(TAG, "DCC Protocol is null, cannot toggle track power");
      return;
    }

    if (trackPowerState == DCCExController::PowerOn) {
      ESP_LOGI(TAG, "MainTrack power OFF requested");
      dccProtocol->powerMainOff();
    } else {
      ESP_LOGI(TAG, "Main Track power ON requested");
      dccProtocol->powerMainOn();
    }
  }
}

// Disconnects from the DCC server, cleans up this screen and returns to the
// main (FirstScreen) home screen.
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
