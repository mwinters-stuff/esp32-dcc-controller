/**
 * @file FirstScreen.cpp
 * @brief Home screen shown after boot, displaying WiFi status and navigation
 *        buttons for Connect DCC, Scan WiFi, Calibrate, and Settings.
 */
#include "FirstScreen.h"
#include "ConnectDCC.h"
#include "LvglWrapper.h"
#include "ManualCalibration.h"
#include "SettingsScreen.h"
#include "WifiListScreen.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "FIRST_SCREEN";

void FirstScreen::maybeAutoConnectSavedDccFromMain() {
  auto *self = this;
  if (!self)
    return;

  if (display::ConnectDCCScreen::isBootAutoConnectHandled()) {
    ESP_LOGI(TAG, "Skipping boot DCC auto-connect: already handled");
    return;
  }

  auto wifiHandler = utilities::WifiHandler::instance();
  if (!wifiHandler->isConnected()) {
    ESP_LOGI(TAG, "Skipping boot DCC auto-connect: WiFi not connected");
    return;
  }

  utilities::WithrottleDevice savedDevice;
  if (!display::ConnectDCCScreen::loadSavedConnection(savedDevice)) {
    ESP_LOGI(TAG, "Skipping boot DCC auto-connect: no saved DCC connection");
    display::ConnectDCCScreen::markBootAutoConnectHandled();
    return;
  }

  ESP_LOGI(TAG, "Saved DCC connection found. Opening Connect DCC screen for auto-connect.");
  self->cleanUp();
  auto connectScreen = display::ConnectDCCScreen::instance();
  connectScreen->showScreen(display::FirstScreen::instance());
  connectScreen->maybeAutoConnectSaved();
}

void FirstScreen::wifi_connected_callback(lv_msg_t *msg) {
  (void)msg;
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

void FirstScreen::wifi_not_saved_callback(lv_msg_t *msg) {
  (void)msg;
  if (isCleanedUp)
    return;
  enableButtons(false);
  ESP_LOGI(TAG, "No wifi details saved");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Not Configured");
  if (lbl_ip)
    lv_label_set_text(lbl_ip, "");
}

void FirstScreen::enableButtons(bool enableConnect) {
  if (isCleanedUp)
    return;

  ESP_LOGI(TAG, "EnableButtons");
  if (!enableConnect) {
    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
  }
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_wifi_scan, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_settings, LV_STATE_DISABLED);
}

void FirstScreen::disableButtons() {
  if (isCleanedUp)
    return;

  ESP_LOGI(TAG, "DisableButtons");
  lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_add_state(btn_wifi_scan, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_settings, LV_STATE_DISABLED);
}

void FirstScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  (void)parentScreen;
  isCleanedUp = false;
  lv_obj_clean(lvObj_);

  lbl_title = makeLabel(lvObj_, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, "label.title", &lv_font_montserrat_30);

  btn_connect = makeButton(lvObj_, "Connect", 220, 42, LV_ALIGN_CENTER, 0, -90, "button.primary");
  lv_obj_add_event_cb(btn_connect, &FirstScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  btn_wifi_scan = makeButton(lvObj_, "Scan WiFi", 220, 42, LV_ALIGN_CENTER, 0, -35, "button.primary");
  lv_obj_add_event_cb(btn_wifi_scan, &FirstScreen::event_wifi_list_trampoline, LV_EVENT_CLICKED, this);

  btn_cal = makeButton(lvObj_, "Calibrate", 220, 42, LV_ALIGN_CENTER, 0, 20, "button.secondary");
  lv_obj_add_event_cb(btn_cal, &FirstScreen::event_calibrate_trampoline, LV_EVENT_CLICKED, this);

  btn_settings = makeButton(lvObj_, "Settings", 220, 42, LV_ALIGN_CENTER, 0, 75, "button.secondary");
  lv_obj_add_event_cb(btn_settings, &FirstScreen::event_settings_trampoline, LV_EVENT_CLICKED, this);

  disableButtons();

  bool connected = utilities::WifiHandler::instance()->isConnected();
  auto ip = utilities::WifiHandler::instance()->getIpAddress();

  lbl_status = makeLabel(lvObj_, connected ? "WiFi Connected" : "WiFi Connecting...", LV_ALIGN_CENTER, 0, 135, "label.main");
  lbl_ip = makeLabel(lvObj_, ip.empty() ? "" : ip.c_str(), LV_ALIGN_CENTER, 0, 165, "label.muted");

  subscribe_connected = lv_msg_subscribe(MSG_WIFI_CONNECTED, &FirstScreen::wifi_connected_trampoline, this);
  subscribe_not_saved = lv_msg_subscribe(MSG_WIFI_NOT_SAVED, &FirstScreen::wifi_not_saved_trampoline, this);

  focusedIndex = 0;
  updateFocusedState();
  rotaryAttach();
  ESP_LOGI(TAG, "FirstScreen UI created (C LVGL)");

  if (!connected) {
    enableButtons(false);
  } else {
    enableButtons(true);
    maybeAutoConnectSavedDccFromMain();
  }
}

void FirstScreen::cleanUp() {
  ESP_LOGI(TAG, "FirstScreen cleaned up");
  isCleanedUp = true;
  rotaryDetach();

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
  btn_settings = nullptr;
  lbl_status = nullptr;
  lbl_ip = nullptr;
  focusedIndex = -1;

  lv_obj_clean(lvObj_);
}

void FirstScreen::moveFocus(int direction) {
  if (isCleanedUp || direction == 0) {
    return;
  }

  constexpr int total = 4;
  int idx = focusedIndex;
  if (idx < 0 || idx >= total) {
    idx = 0;
  } else {
    idx = (idx + direction) % total;
    if (idx < 0) {
      idx += total;
    }
  }
  focusedIndex = idx;
  updateFocusedState();
}

void FirstScreen::updateFocusedState() {
  applyFocusOutline(btn_connect, focusedIndex == 0);
  applyFocusOutline(btn_wifi_scan, focusedIndex == 1);
  applyFocusOutline(btn_cal, focusedIndex == 2);
  applyFocusOutline(btn_settings, focusedIndex == 3);
}

void FirstScreen::rotaryMoveFocus(int direction) { moveFocus(direction); }

void FirstScreen::rotaryActivateFocused() {
  if (isCleanedUp) {
    return;
  }

  if (focusedIndex == 0 && btn_connect && !lv_obj_has_state(btn_connect, LV_STATE_DISABLED)) {
    lv_obj_send_event(btn_connect, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 1 && btn_wifi_scan && !lv_obj_has_state(btn_wifi_scan, LV_STATE_DISABLED)) {
    lv_obj_send_event(btn_wifi_scan, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 2 && btn_cal && !lv_obj_has_state(btn_cal, LV_STATE_DISABLED)) {
    lv_obj_send_event(btn_cal, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 3 && btn_settings && !lv_obj_has_state(btn_settings, LV_STATE_DISABLED)) {
    lv_obj_send_event(btn_settings, LV_EVENT_CLICKED, nullptr);
  }
}

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

void FirstScreen::button_settings_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;

  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Settings button clicked!");
  cleanUp();
  auto settingsScreen = SettingsScreen::instance();
  settingsScreen->showScreen(FirstScreen::instance());
}

} // namespace display
