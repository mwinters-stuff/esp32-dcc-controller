/**
 * @file FirstScreen.cpp
 * @brief Home screen shown after boot, displaying WiFi status and navigation
 *        buttons for Connect DCC, Scan WiFi and Calibrate.
 *
 * Subscribes to WiFi state messages to enable/disable the Connect button in
 * real time. On first WiFi connection also attempts a boot auto-connect to the
 * previously saved DCC server.
 */
#include "FirstScreen.h"
#include "ConnectDCC.h"
#include "LvglWrapper.h"
#include "ManualCalibration.h"
#include "WaitingScreen.h"
#include "WifiListScreen.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "FIRST_SCREEN";

// On the first WiFi-up event after boot, checks whether a saved DCC server
// exists and, if so, opens ConnectDCCScreen and starts the boot-only
// auto-connect flow.
void FirstScreen::maybeAutoConnectSavedDccFromMain() {
  auto *self = this;
  if (!self)
    return;

  if (display::ConnectDCCScreen::isBootAutoConnectHandled()) {
    return;
  }

  auto wifiHandler = utilities::WifiHandler::instance();
  if (!wifiHandler->isConnected()) {
    return;
  }

  auto wifiControl = utilities::WifiControl::instance();
  if (wifiControl->connectionState() == utilities::WifiControl::CONNECTING ||
      wifiControl->connectionState() == utilities::WifiControl::CONNECTED) {
    display::ConnectDCCScreen::markBootAutoConnectHandled();
    return;
  }

  utilities::WithrottleDevice savedDevice;
  if (!display::ConnectDCCScreen::loadSavedConnection(savedDevice)) {
    display::ConnectDCCScreen::markBootAutoConnectHandled();
    return;
  }

  ESP_LOGI(TAG, "Saved DCC connection found. Opening Connect DCC screen for auto-connect.");
  self->cleanUp();
  auto connectScreen = display::ConnectDCCScreen::instance();
  connectScreen->showScreen(display::FirstScreen::instance());
  connectScreen->maybeAutoConnectSaved();
}

// Called when MSG_WIFI_CONNECTED is received. Updates the status labels,
// enables the Connect button and triggers the boot auto-connect check.
void FirstScreen::wifi_connected_callback(lv_msg_t *msg) {
  if (isCleanedUp)
    return;
  enableButtons(true);
  ESP_LOGI(TAG, "Connected to wifi");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Connected");
  if (lbl_ip) {
    if (auto ip = utilities::WifiHandler::instance()->getIpAddress(); !ip.empty())
      lv_label_set_text(lbl_ip, ip.c_str());
  }

  maybeAutoConnectSavedDccFromMain();
}

// Called when MSG_WIFI_NOT_SAVED is received. Clears the IP label and disables
// the Connect button since there is no network to reach a DCC server on.
void FirstScreen::wifi_not_saved_callback(lv_msg_t *msg) {
  if (isCleanedUp)
    return;
  enableButtons(false);
  ESP_LOGI(TAG, "No wifi details saved");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Not Configured");
  if (lbl_ip)
    lv_label_set_text(lbl_ip, "");
}

// Enables or disables the Connect DCC button. The Calibrate and Scan WiFi
// buttons are always enabled regardless of the enableConnect flag.
void FirstScreen::enableButtons(bool enableConnect) {
  if (isCleanedUp)
    return;

  ESP_LOGI(TAG, "EnableButtons");
  // Enable or disable buttons (C LVGL)
  if (!enableConnect) {
    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
  }
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_wifi_scan, LV_STATE_DISABLED);
}

// Disables all navigation buttons; used during initial screen setup before the
// WiFi state is known.
void FirstScreen::disableButtons() {
  if (isCleanedUp)
    return;

  ESP_LOGI(TAG, "DisableButtons");
  // Disable all buttons (C LVGL)
  lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_add_state(btn_wifi_scan, LV_STATE_DISABLED);
}

// Builds the home screen UI: title, Connect/Scan WiFi/Calibrate buttons and
// status labels. Subscribes to WiFi messages and immediately reflects current
// connection state.
void FirstScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;
  lv_obj_clean(lvObj_);

  // Title Label
  lbl_title = makeLabel(lvObj_, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, "label.title", &lv_font_montserrat_30);

  // "Connect" button
  btn_connect = makeButton(lvObj_, "Connect", 200, 48, LV_ALIGN_CENTER, 0, -60, "button.primary");
  lv_obj_add_event_cb(btn_connect, &FirstScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  // "Scan WiFi" button
  btn_wifi_scan = makeButton(lvObj_, "Scan WiFi", 200, 48, LV_ALIGN_CENTER, 0, 0, "button.primary");
  lv_obj_add_event_cb(btn_wifi_scan, &FirstScreen::event_wifi_list_trampoline, LV_EVENT_CLICKED, this);

  // "Calibrate" button
  btn_cal = makeButton(lvObj_, "Calibrate", 200, 48, LV_ALIGN_CENTER, 0, 60, "button.secondary");
  lv_obj_add_event_cb(btn_cal, &FirstScreen::event_calibrate_trampoline, LV_EVENT_CLICKED, this);

  disableButtons();

  // Status labels under the last button
  bool connected = utilities::WifiHandler::instance()->isConnected();
  auto ip = utilities::WifiHandler::instance()->getIpAddress();

  lbl_status =
      makeLabel(lvObj_, connected ? "WiFi Connected" : "WiFi Connecting...", LV_ALIGN_CENTER, 0, 120, "label.main");
  lbl_ip = makeLabel(lvObj_, ip.empty() ? "" : ip.c_str(), LV_ALIGN_CENTER, 0, 150, "label.muted");

  // Subscribe to WiFi messages
  subscribe_connected = lv_msg_subscribe(MSG_WIFI_CONNECTED, &FirstScreen::wifi_connected_trampoline, this);
  subscribe_not_saved = lv_msg_subscribe(MSG_WIFI_NOT_SAVED, &FirstScreen::wifi_not_saved_trampoline, this);
  ESP_LOGI(TAG, "FirstScreen UI created (C LVGL)");
  if (!connected) {
    enableButtons(false);
  } else {
    enableButtons(true);
    maybeAutoConnectSavedDccFromMain();
  }
}

// Removes message subscriptions and nulls widget pointers. Must be called
// before navigating to another screen.
void FirstScreen::cleanUp() {
  ESP_LOGI(TAG, "FirstScreen cleaned up");
  isCleanedUp = true;

  if (subscribe_connected != nullptr) {
    lv_msg_unsubscribe(subscribe_connected);
    subscribe_connected = nullptr;
  }
  if (subscribe_not_saved != nullptr) {
    lv_msg_unsubscribe(subscribe_not_saved);
    subscribe_not_saved = nullptr;
  }

  lbl_title = nullptr;
  btn_connect = nullptr;
  btn_wifi_scan = nullptr;
  btn_cal = nullptr;
  lbl_status = nullptr;
  lbl_ip = nullptr;

  lv_obj_clean(lvObj_);
}

// Opens the ConnectDCC screen.
void FirstScreen::button_connect_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Connect button clicked!");
  cleanUp();
  auto connectDCCScreen = ConnectDCCScreen::instance();
  connectDCCScreen->showScreen(FirstScreen::instance());
}

// Opens the WiFi network scan/selection screen.
void FirstScreen::button_wifi_list_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;

  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Scan WiFi button clicked!");
  cleanUp();
  auto wifiScreen = WifiListScreen::instance();
  wifiScreen->showScreen(FirstScreen::instance());
}

// Opens the touch-screen calibration screen.
void FirstScreen::button_calibrate_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;

  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Calibrate button clicked!");
  cleanUp();
  auto calScreen = ManualCalibration::instance();
  calScreen->showScreen(FirstScreen::instance());
}

} // namespace display
