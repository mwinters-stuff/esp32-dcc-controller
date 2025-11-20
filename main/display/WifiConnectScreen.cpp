#include "WifiConnectScreen.h"
#include "FirstScreen.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <nvs_flash.h>
#include <nvs_handle.hpp>

#include <memory>

extern EventGroupHandle_t wifi_event_group;

namespace display {
static const char *TAG = "WIFI_CONNECT_SCREEN";

static void wifi_connected_cb(void *s, lv_msg_t *msg) {
  WifiConnectScreen *self = static_cast<WifiConnectScreen *>(msg->user_data);
  self->lbl_status_->setText("Connected!");
  self->lbl_spinner_->setVisible(false);

  // Close after short delay
  lv_timer_create(
      [](lv_timer_t *t) {
        WifiConnectScreen *self = static_cast<WifiConnectScreen *>(t->user_data);
        self->unsubscribeAll();
        FirstScreen::instance()->showScreen();

        ESP_ERROR_CHECK(utilities::WifiHandler::instance()->saveConfiguration());

        lv_timer_del(t);
      },
      1500, self);
}

static void wifi_failed_cb(void *s, lv_msg_t *msg) {
  WifiConnectScreen *self = static_cast<WifiConnectScreen *>(msg->user_data);
  self->lbl_status_->setText("Connection Failed");
  self->lbl_spinner_->setVisible(false);
  self->kb_keyboard_->setVisible(true);
}

void WifiConnectScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  createScreen();
  wifi_connected_sub_ = lv_msg_subscribe(MSG_WIFI_CONNECTED, wifi_connected_cb, this);
  wifi_failed_sub_ = lv_msg_subscribe(MSG_WIFI_FAILED, wifi_failed_cb, this);
}

void WifiConnectScreen::unsubscribeAll() {
  if (wifi_connected_sub_) {
    lv_msg_unsubscribe(wifi_connected_sub_);
    wifi_connected_sub_ = nullptr;
  }
  if (wifi_failed_sub_) {
    lv_msg_unsubscribe(wifi_failed_sub_);
    wifi_failed_sub_ = nullptr;
  }
}

void WifiConnectScreen::cleanUp() {
  ESP_LOGI(TAG, "WifiConnectScreen cleaned up");

  // lbl_title_.reset();
  // lbl_subtitle_.reset();
  // vert_container_.reset();
  // lbl_pwd_.reset();
  // pwd_container_.reset();
  // ta_password_.reset();
  // bs_password_show_.reset();
  // kb_keyboard_.reset();
  // lbl_status_.reset();
  // lbl_spinner_.reset();
  // Screen::cleanUp();
}

void WifiConnectScreen::setSSID(const std::string &ssid) { ssid_ = ssid; }

void WifiConnectScreen::createScreen() {

  lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES);
  lv_obj_center(lvObj_);

  lbl_title_ = std::make_unique<LvglLabel>(lvObj_, "Connecting To", LV_ALIGN_TOP_MID, 0, 8);
  lbl_title_->setStyle("label.title");

  lbl_subtitle_ = std::make_unique<LvglLabel>(lvObj_, ssid_.c_str(), LV_ALIGN_TOP_MID, 0, 40);
  lbl_subtitle_->setStyle("label.main");

  // Create a vertical container for label + password row
  vert_container_ = std::make_unique<LvglVerticalLayout>(lvObj_, 300, LV_SIZE_CONTENT);
  vert_container_->setAlignment(LV_ALIGN_TOP_LEFT, 10, 80);

  lbl_pwd_ = std::make_unique<LvglLabel>(vert_container_->lvObj(), "Password:");

  pwd_container_ = std::make_unique<LvglHorizontalLayout>(vert_container_->lvObj());

  // // Create the password text area
  ta_password_ = std::make_unique<LvglTextArea>(pwd_container_->lvObj(), "Enter Password", true, true);

  //     // Create the show/hide button
  bs_password_show_ = std::make_unique<LvglButtonSymbol>(pwd_container_->lvObj(), LV_SYMBOL_EYE_OPEN, 40, 40, true);

  bs_password_show_->setCallback([](lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
      lv_obj_t *btn = lv_event_get_target(e);
      bool checked = lv_obj_has_state(btn, LV_STATE_CHECKED);
      lv_obj_t *row = lv_obj_get_parent(btn);
      lv_obj_t *ta = lv_obj_get_child(row, 0);
      lv_obj_t *lbl = lv_obj_get_child(btn, 0);
      lv_textarea_set_password_mode(ta, !checked);
      lv_label_set_text(lbl, checked ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    }
  });

  // // Keyboard (hidden by default)
  // lv_obj_t *layer_top = lv_layer_top(); // LVGL’s dedicated overlay layer
  kb_keyboard_ = std::make_unique<LvglKeyboard>(lvObj_, [this](lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    // lv_obj_t *kb = lv_event_get_target(e);
    // lv_obj_t *ta = lv_keyboard_get_textarea(kb);

    if (code == LV_EVENT_READY) {
      // OK / Enter pressed
      kb_keyboard_->setVisible(false);

      lbl_status_->setText("Connecting...");
      lbl_status_->setVisible(true);
      lbl_spinner_->setVisible(true);

      auto password = ta_password_->getText();

      ESP_LOGI(TAG, "Connecting to SSID: %s", ssid_.c_str());
      utilities::WifiHandler::instance()->create_wifi_connect_task(ssid_.c_str(), password.c_str());
    } else if (code == LV_EVENT_CANCEL) {
      // Close button pressed (×)
      kb_keyboard_->setVisible(false);
      unsubscribeAll();
      ESP_LOGI(TAG, "Connection cancelled for SSID: %s", ssid_.c_str());
      if (auto screen = parentScreen_.lock()) {
        screen->showScreen();
      }
    }
  });

  kb_keyboard_->setVisible(true);
  kb_keyboard_->setTextArea(ta_password_->lvObj());

  lbl_status_ = std::make_unique<LvglLabel>(lv_scr_act(), "", LV_ALIGN_TOP_MID, 0, 100);
  lbl_status_->setVisible(false);

  lbl_spinner_ = std::make_unique<LvglSpinner>(lv_scr_act());
  lbl_spinner_->hideSpinner();
}

} // namespace display