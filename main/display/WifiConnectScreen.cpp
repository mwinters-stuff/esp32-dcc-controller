/**
 * @file WifiConnectScreen.cpp
 * @brief Screen for entering a WiFi password and connecting to a selected SSID.
 *
 * Shows an SSID label, a password text area with show/hide toggle, and a
 * keyboard. Subscribes to MSG_WIFI_CONNECTED / MSG_WIFI_CONNECTION_FAILED to
 * update the UI or show an error and return to WifiListScreen.
 */
#include "WifiConnectScreen.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <nvs_handle.hpp>

#include <memory>

extern EventGroupHandle_t wifi_event_group;

namespace display {
static const char *TAG = "WIFI_CONNECT_SCREEN";

// Called when MSG_WIFI_CONNECTED fires. Updates the status label and
// navigates back to FirstScreen after a brief delay.
void WifiConnectScreen::wifi_connected_callback(lv_msg_t *msg) {
  if (isCleanedUp)
    return;
  lv_label_set_text(lbl_status, "Connected!");
  lv_obj_clear_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);

  // Close after short delay
  lv_timer_create(
      [](lv_timer_t *t) {
        WifiConnectScreen *self = static_cast<WifiConnectScreen *>(lv_timer_get_user_data(t));
        if (!self || self->isCleanedUp)
          return;

        // Defer cleanup/navigation to avoid tearing down UI in timer callback context.
        lv_async_call(
            [](void *arg) {
              auto *screen = static_cast<WifiConnectScreen *>(arg);
              if (!screen || screen->isCleanedUp)
                return;
              screen->cleanUp();
              FirstScreen::instance()->showScreen();
            },
            self);

        // Offload NVS flash write to a background task — it blocks for several ms
        // and must not run inside the LVGL timer handler.
        xTaskCreate(
            [](void *) {
              utilities::WifiHandler::instance()->saveConfiguration();
              vTaskDelete(nullptr);
            },
            "wifi_save_cfg", 2048, nullptr, tskIDLE_PRIORITY, nullptr);

        lv_timer_delete(t);
      },
      1500, this);
}

// Creates the screen UI and sets up message subscriptions.
void WifiConnectScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;
  createScreen();
  wifi_connected_sub = lv_msg_subscribe(MSG_WIFI_CONNECTED, &WifiConnectScreen::wifi_connected_trampoline, this);

  wifi_failed_sub = lv_msg_subscribe(MSG_WIFI_FAILED, &WifiConnectScreen::wifi_failed_trampoline, this);

  reshow_screen_sub = lv_msg_subscribe(
      MSG_RESHOW_WIFI_CONNECT_SCREEN,
      [](lv_msg_t *msg) {
        auto *self = static_cast<WifiConnectScreen *>(lv_msg_get_user_data(msg));
        if (!self)
          return;
        auto *payload = static_cast<const utilities::ReshowScreenData *>(lv_msg_get_payload(msg));
        if (payload) {
          self->reshowScreen(payload->status, payload->subtitle);
        }
      },
      this);
}

// Refreshes the on-screen status and subtitle labels without rebuilding the
// full UI; used after a connection failure to prompt retry.
void WifiConnectScreen::reshowScreen(const std::string &status, const std::string &subtitle) {
  if (isCleanedUp)
    return;

  if (!subtitle.empty()) {
    lv_label_set_text(lbl_status2, subtitle.c_str());
    lv_obj_clear_flag(lbl_status2, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(lbl_status2, LV_OBJ_FLAG_HIDDEN);
  }

  lv_label_set_text(lbl_status, status.c_str());
  lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
}

// Called when MSG_WIFI_CONNECTION_FAILED fires. Shows the error message and
// re-enables the keyboard so the user can try again.
void WifiConnectScreen::wifi_failed_callback(lv_msg_t *msg) {
  if (isCleanedUp)
    return;

  auto *payload = static_cast<const WifiFailedPayload *>(lv_msg_get_payload(msg));
  if (payload == nullptr || !payload->suppressGlobalPopup || payload->source != WifiFailedSource::ConnectAttempt)
    return;

  lv_label_set_text(lbl_status, "Connection Failed");
  lv_obj_add_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
}

// Releases widget pointers and removes message subscriptions.
void WifiConnectScreen::cleanUp() {
  ESP_LOGI(TAG, "WifiConnectScreen cleaned up");
  isCleanedUp = true;

  if (wifi_connected_sub) {
    lv_msg_unsubscribe(wifi_connected_sub);
    wifi_connected_sub = nullptr;
  }
  if (wifi_failed_sub) {
    lv_msg_unsubscribe(wifi_failed_sub);
    wifi_failed_sub = nullptr;
  }
  if (reshow_screen_sub) {
    lv_msg_unsubscribe(reshow_screen_sub);
    reshow_screen_sub = nullptr;
  }
  lv_obj_clean(lvObj_);

  lbl_title = nullptr;
  lbl_subtitle = nullptr;
  vert_container = nullptr;
  lbl_pwd = nullptr;
  pwd_container = nullptr;
  ta_password = nullptr;
  bs_password_show = nullptr;
  lbl_status = nullptr;
  lbl_spinner = nullptr;
  kb_keyboard = nullptr;
  lbl_status2 = nullptr;
}

// Stores the target SSID before show() is called.
void WifiConnectScreen::setSSID(const std::string &ssid) { this->ssid = ssid; }

// Builds the full screen layout: SSID label, password text area, show/hide
// toggle button, connect button and on-screen keyboard.
void WifiConnectScreen::createScreen() {

  lv_obj_set_size(lvObj_, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
  lv_obj_center(lvObj_);

  lbl_title = makeLabel(lvObj_, "Connecting To", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_22);

  lbl_subtitle = makeLabel(lvObj_, ssid.c_str(), LV_ALIGN_TOP_MID, 0, 40, "label.main");

  // Create a vertical container for label + password row
  vert_container = makeVerticalLayout(lvObj_, 300, LV_SIZE_CONTENT);
  lv_obj_align(vert_container, LV_ALIGN_TOP_LEFT, 10, 80);

  lbl_pwd = makeLabel(vert_container, "Password:", LV_ALIGN_TOP_LEFT, 0, 0, "label.main");

  pwd_container = makeHorizontalLayout(vert_container, 300, 40);

  // // Create the password text area
  ta_password = makeTextArea(pwd_container, "Enter Password", true, true);
  lv_obj_set_style_text_font(ta_password, &lv_font_montserrat_20, 0);

  //     // Create the show/hide button
  bs_password_show = makeButtonSymbol(pwd_container, LV_SYMBOL_EYE_OPEN, 40, 40, true);
  lv_obj_set_flex_grow(bs_password_show, 0); // Button takes only necessary space
  lv_obj_add_event_cb(bs_password_show, &WifiConnectScreen::event_password_show_trampoline, LV_EVENT_VALUE_CHANGED,
                      this);

  // // Keyboard (hidden by default)
  // lv_obj_t *layer_top = lv_layer_top(); // LVGL’s dedicated overlay layer
  kb_keyboard = makeKeyboard(lvObj_);

  lv_obj_add_event_cb(kb_keyboard, &WifiConnectScreen::event_keyboard_trampoline, LV_EVENT_ALL, this);

  lv_obj_clear_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(kb_keyboard, ta_password);

  lbl_status = makeLabel(vert_container, "", LV_ALIGN_TOP_MID, 0, 100, "label.main", &lv_font_montserrat_20);
  lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);

  lbl_status2 = makeLabel(vert_container, "", LV_ALIGN_TOP_MID, 0, 130, "label.main", &lv_font_montserrat_16);
  lv_label_set_long_mode(lbl_status2, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_status2, lv_pct(100));
  lv_obj_add_flag(lbl_status2, LV_OBJ_FLAG_HIDDEN);

  lbl_spinner = makeSpinner(lv_screen_active(), 0, 0, 40, 1000);
  lv_obj_add_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
}

// Toggles password visibility between plain text and masked mode.
void WifiConnectScreen::event_password_show_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    bool checked = lv_obj_has_state(btn, LV_STATE_CHECKED);
    lv_obj_t *row = lv_obj_get_parent(btn);
    lv_obj_t *ta = lv_obj_get_child(row, 0);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    lv_textarea_set_password_mode(ta, !checked);
    lv_label_set_text(lbl, checked ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
  }
}

// Returns true when the password is currently displayed as plain text.
bool WifiConnectScreen::isPasswordVisible() { return !lv_textarea_get_password_mode(ta_password); }

// Handles on-screen keyboard events: on LV_EVENT_READY starts the WiFi
// connection attempt using the entered password.
void WifiConnectScreen::event_keyboard_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    // OK / Enter pressed
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);

    if (isPasswordVisible()) {
      // If password is currently visible, hide it before connecting
      lv_textarea_set_password_mode(ta_password, true);
      lv_label_set_text(bs_password_show, LV_SYMBOL_EYE_OPEN);
    }

    lv_label_set_text(lbl_status, "Connecting...");
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_status2, LV_OBJ_FLAG_HIDDEN);

    auto password = std::string(lv_textarea_get_text(ta_password));

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    utilities::WifiHandler::instance()->create_wifi_connect_task(ssid.c_str(), password.c_str(), true);
  } else if (code == LV_EVENT_CANCEL) {
    // Close button pressed (×)
    lv_obj_add_flag(kb_keyboard, LV_OBJ_FLAG_HIDDEN);

    auto *ctx = new std::pair<WifiConnectScreen *, std::weak_ptr<Screen>>(this, parentScreen_);
    lv_async_call(
        [](void *arg) {
          auto *data = static_cast<std::pair<WifiConnectScreen *, std::weak_ptr<Screen>> *>(arg);
          auto *self = data->first;
          auto parent = data->second;
          delete data;
          if (!self || self->isCleanedUp)
            return;
          self->cleanUp();
          if (auto screen = parent.lock()) {
            screen->showScreen();
          }
        },
        ctx);

    ESP_LOGI(TAG, "Connection cancelled for SSID: %s", ssid.c_str());
  }
}

} // namespace display