#include "WifiConnectScreen.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <nvs_flash.h>
#include <nvs_handle.hpp>

#include <memory>

extern EventGroupHandle_t wifi_event_group;

namespace display {
static const char *TAG = "WIFI_CONNECT_SCREEN";

void WifiConnectScreen::wifi_connected_callback(void *s, lv_msg_t *msg) {
  lv_label_set_text(lbl_status, "Connected!");
  lv_obj_clear_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);

  // Close after short delay
  lv_timer_create(
      [](lv_timer_t *t) {
        WifiConnectScreen *self = static_cast<WifiConnectScreen *>(t->user_data);
        self->unsubscribeAll();
        FirstScreen::instance()->showScreen();

        ESP_ERROR_CHECK(utilities::WifiHandler::instance()->saveConfiguration());

        lv_timer_del(t);
      },
      1500, this);
}

void WifiConnectScreen::wifi_failed_callback(void *s, lv_msg_t *msg) {
  lv_label_set_text(lbl_status, "Connection Failed");
  lv_obj_add_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void WifiConnectScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  createScreen();
  wifi_connected_sub = lv_msg_subscribe(MSG_WIFI_CONNECTED, &WifiConnectScreen::wifi_connected_trampoline, this);
  wifi_failed_sub = lv_msg_subscribe(MSG_WIFI_FAILED, &WifiConnectScreen::wifi_failed_trampoline, this);
}

void WifiConnectScreen::unsubscribeAll() {
  if (wifi_connected_sub) {
    lv_msg_unsubscribe(wifi_connected_sub);
    wifi_connected_sub = nullptr;
  }
  if (wifi_failed_sub) {
    lv_msg_unsubscribe(wifi_failed_sub);
    wifi_failed_sub = nullptr;
  }
}

void WifiConnectScreen::cleanUp() {
  ESP_LOGI(TAG, "WifiConnectScreen cleaned up");
  lv_obj_clean(lvObj_);
}

void WifiConnectScreen::setSSID(const std::string &ssid) { this->ssid = ssid; }

void WifiConnectScreen::createScreen() {

  lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES);
  lv_obj_center(lvObj_);

  lbl_title = makeLabel(lvObj_, "Connecting To", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_22);

  lbl_subtitle = makeLabel(lvObj_, ssid.c_str(), LV_ALIGN_TOP_MID, 0, 40, "label.main");

  // Create a vertical container for label + password row
  vert_container = makeVerticalLayout(lvObj_, 300, LV_SIZE_CONTENT);
  lv_obj_align(vert_container, LV_ALIGN_TOP_LEFT, 10, 80);

  lbl_pwd = makeLabel(vert_container, "Password:", LV_ALIGN_TOP_LEFT, 0, 0, "label.main");

  pwd_container = makeHorizontalLayout(vert_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  // // Create the password text area
  ta_password = makeTextArea(pwd_container, "Enter Password", true, true);
  lv_obj_set_height(ta_password, 40);
  lv_obj_set_style_text_font(ta_password, &lv_font_montserrat_20, 0);

  //     // Create the show/hide button
  bs_password_show = makeButtonSymbol(pwd_container, LV_SYMBOL_EYE_OPEN, 40, 40, true);
  lv_obj_add_event_cb(bs_password_show, &WifiConnectScreen::event_password_show_trampoline, LV_EVENT_VALUE_CHANGED,
                      this);

  // // Keyboard (hidden by default)
  // lv_obj_t *layer_top = lv_layer_top(); // LVGL’s dedicated overlay layer
  kb_keyboard = makeKeyboard(lvObj_);

  lv_obj_add_event_cb(kb_keyboard, &WifiConnectScreen::event_keyboard_trampoline, LV_EVENT_ALL, this);

  lv_obj_clear_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(kb_keyboard, ta_password);

  lbl_status = makeLabel(vert_container, "", LV_ALIGN_TOP_MID, 0, 100);
  lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);

  lbl_spinner = makeSpinner(lv_scr_act(), 0, 0, 40, 1000);
  lv_obj_add_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
}

void WifiConnectScreen::event_password_show_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *btn = lv_event_get_target(e);
    bool checked = lv_obj_has_state(btn, LV_STATE_CHECKED);
    lv_obj_t *row = lv_obj_get_parent(btn);
    lv_obj_t *ta = lv_obj_get_child(row, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_textarea_set_password_mode(ta, !checked);
    lv_label_set_text(lbl, checked ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
  }
}

void WifiConnectScreen::event_keyboard_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    // OK / Enter pressed
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(lbl_status, "Connecting...");
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);

    auto password = std::string(lv_textarea_get_text(ta_password));

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    utilities::WifiHandler::instance()->create_wifi_connect_task(ssid.c_str(), password.c_str());
  } else if (code == LV_EVENT_CANCEL) {
    // Close button pressed (×)
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
    unsubscribeAll();
    ESP_LOGI(TAG, "Connection cancelled for SSID: %s", ssid.c_str());
    if (auto screen = parentScreen_.lock()) {
      screen->showScreen();
    }
  }
}

} // namespace display