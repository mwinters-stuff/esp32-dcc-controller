#include "FirstScreen.h"
#include "ConnectDCC.h"
#include "ManualCalibration.h"
#include "WaitingScreen.h"
#include "WifiListScreen.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>
#include "LvglWrapper.h"

namespace display {
static const char *TAG = "FIRST_SCREEN";

void FirstScreen::wifi_connected_callback(void *s, lv_msg_t *msg) {
  enableButtons(true);
  ESP_LOGI(TAG, "Connected to wifi");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Connected");
  if (lbl_ip) {
    if (auto ip = utilities::WifiHandler::instance()->getIpAddress(); !ip.empty())
      lv_label_set_text(lbl_ip, ip.c_str());
  }
}

void FirstScreen::wifi_failed_callback(void *s, lv_msg_t *msg) {
  enableButtons(false);
  ESP_LOGI(TAG, "Not connected to wifi");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Disconnected");
  if (lbl_ip)
    lv_label_set_text(lbl_ip, "");
}

void FirstScreen::wifi_not_saved_callback(void *s, lv_msg_t *msg) {
  enableButtons(false);
  ESP_LOGI(TAG, "No wifi details saved");
  if (lbl_status)
    lv_label_set_text(lbl_status, "WiFi Not Configured");
  if (lbl_ip)
    lv_label_set_text(lbl_ip, "");
}

void FirstScreen::enableButtons(bool enableConnect) {
  ESP_LOGI(TAG, "EnableButtons");
  // Enable or disable buttons (C LVGL)
  lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_clear_state(btn_wifi_scan, LV_STATE_DISABLED);
  if (!enableConnect)
    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
}

void FirstScreen::disableButtons() {
  ESP_LOGI(TAG, "DisableButtons");
  // Disable all buttons (C LVGL)
  lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_add_state(btn_cal, LV_STATE_DISABLED);
  lv_obj_add_state(btn_wifi_scan, LV_STATE_DISABLED);
}

void FirstScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  // Title Label
  lbl_title = makeLabel(lvObj_, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, "label.title", &lv_font_montserrat_30);


  // "Connect" button
  btn_connect = makeButton(lvObj_, "Connect", 200, 48, LV_ALIGN_CENTER, 0, -60, "button.primary");
  lv_obj_add_event_cb(btn_connect, &FirstScreen::event_connect_trampoline, LV_EVENT_ALL, this);

    // "Scan WiFi" button
  btn_wifi_scan = makeButton(lvObj_, "Scan WiFi", 200, 48, LV_ALIGN_CENTER, 0, 0, "button.primary");
  lv_obj_add_event_cb(btn_wifi_scan, &FirstScreen::event_wifi_list_trampoline, LV_EVENT_ALL, this);
  lv_obj_add_event_cb(btn_wifi_scan, &FirstScreen::event_wifi_list_trampoline, LV_EVENT_ALL, this);

  // "Calibrate" button
  btn_cal = makeButton(lvObj_, "Calibrate", 200, 48, LV_ALIGN_CENTER, 0, 60, "button.secondary");
  lv_obj_add_event_cb(btn_cal, &FirstScreen::event_calibrate_trampoline, LV_EVENT_ALL, this);

  // Status labels under the last button
  auto ip = utilities::WifiHandler::instance()->getIpAddress();
  bool connected = !ip.empty();

  lbl_status = makeLabel(lvObj_, connected ? "WiFi Connected" : "WiFi Connecting...", LV_ALIGN_CENTER, 0, 120, "label.main");
  lbl_ip = makeLabel(lvObj_, ip.empty() ? "" : ip.c_str(), LV_ALIGN_CENTER, 0, 150, "label.muted");

  // Subscribe to WiFi messages
  subscribe_connected = lv_msg_subscribe(MSG_WIFI_CONNECTED, &FirstScreen::wifi_connected_trampoline, this);
  subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, &FirstScreen::wifi_failed_trampoline, this);
  subscribe_not_saved = lv_msg_subscribe(MSG_WIFI_NOT_SAVED, &FirstScreen::wifi_not_saved_trampoline, this);
  ESP_LOGI(TAG, "FirstScreen UI created (C LVGL)");
}

void FirstScreen::unsubscribeAll() {
  if (subscribe_connected != NULL) {
    lv_msg_unsubscribe(subscribe_connected);
    subscribe_connected = NULL;
  }
  if (subscribe_failed != NULL) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = NULL;
  }
  if (subscribe_not_saved != NULL) {
    lv_msg_unsubscribe(subscribe_not_saved);
    subscribe_not_saved = NULL;
  }
}

void FirstScreen::cleanUp() {
  ESP_LOGI(TAG, "FirstScreen cleaned up");
  // In C LVGL, you may want to delete all children of lvObj_ or reset the screen
  lv_obj_clean(lvObj_);
}

void FirstScreen::button_connect_callback(lv_event_t *e) {
  if (e->code != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Connect button clicked!");
  unsubscribeAll();
  auto connectDCCScreen = ConnectDCCScreen::instance();
  connectDCCScreen->showScreen(FirstScreen::instance());
}

void FirstScreen::button_wifi_list_callback(lv_event_t *e) {
  if (e->code != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Scan WiFi button clicked!");
  unsubscribeAll();
  auto wifiScreen = WifiListScreen::instance();
  wifiScreen->showScreen(FirstScreen::instance());
}

void FirstScreen::button_calibrate_callback(lv_event_t *e) {
  if (e->code != LV_EVENT_CLICKED)
    return;
  ESP_LOGI(TAG, "Calibrate button clicked!");
  unsubscribeAll();
  auto calScreen = ManualCalibration::instance();
  calScreen->showScreen(FirstScreen::instance());
}

} // namespace display
