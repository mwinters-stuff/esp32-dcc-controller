#include "FirstScreen.h"
#include "ConnectDCC.h"
#include "LvglWrapper.h"
#include "ManualCalibration.h"
#include "WaitingScreen.h"
#include "WifiListScreen.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "FIRST_SCREEN";

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
}

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

void FirstScreen::disableButtons() {
  if (isCleanedUp)
    return;

  ESP_LOGI(TAG, "DisableButtons");
  // Disable all buttons (C LVGL)
  lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_add_state(btn_wifi_scan, LV_STATE_DISABLED);
}

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
  }
}

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

} // namespace display
