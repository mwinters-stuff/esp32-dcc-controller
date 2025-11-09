#include "WifiConnectScreen.h"
#include "FirstScreen.h"
#include "definitions.h"
#include <nvs_flash.h>
#include <nvs_handle.hpp>

#include <memory>

extern EventGroupHandle_t wifi_event_group;

namespace display
{
  static const char *TAG = "WIFI_CONNECT_SCREEN";

  static void wifi_connected_cb(void *s, lv_msg_t *msg)
  {
    WifiConnectScreen *self = static_cast<WifiConnectScreen *>(msg->user_data);
    lv_label_set_text(self->status_label_, "Connected!");
    lv_obj_add_flag(self->spinner_, LV_OBJ_FLAG_HIDDEN);

    // Close after short delay
    lv_timer_create([](lv_timer_t *t)
                    {
            WifiConnectScreen *self = static_cast<WifiConnectScreen*>(t->user_data);
            FirstScreen::instance()->show();
            
            esp_err_t err;
            std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_WIFI, NVS_READWRITE, &err);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
                return;
            }

            err = handle->set_string(NVS_WIFI_SSID, self->creds->ssid);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "rebootToCalibrate calibration_saved: %s", esp_err_to_name(err));
                return;
            }

            err = handle->set_string(NVS_WIFI_PWD, self->creds->password);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "rebootToCalibrate calibration_saved: %s", esp_err_to_name(err));
                return;
            }

            err = handle->commit();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
                return;
            }


            lv_timer_del(t); }, 1500, self);
  }

  static void wifi_failed_cb(void *s, lv_msg_t *msg)
  {
    WifiConnectScreen *self = static_cast<WifiConnectScreen *>(msg->user_data);
    lv_label_set_text(self->status_label_, "Connection failed");
    lv_obj_add_flag(self->spinner_, LV_OBJ_FLAG_HIDDEN);
  }

  void WifiConnectScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen)
  {
    Screen::show(parent, parentScreen); // Ensure base setup (sets lvObj_, applies theme, etc.)
    createScreen();
    lv_msg_subscribe(MSG_WIFI_CONNECTED, wifi_connected_cb, this);
    lv_msg_subscribe(MSG_WIFI_FAILED, wifi_failed_cb, this);
  }

  void WifiConnectScreen::cleanUp()
  {
    ESP_LOGI(TAG, "WifiConnectScreen cleaned up");
    Screen::cleanUp();
    ssid_label_ = nullptr;
    pwd_textarea_ = nullptr;
    connect_btn_ = nullptr;
    cancel_btn_ = nullptr;
    keyboard_ = nullptr;
    status_label_ = nullptr;
    spinner_ = nullptr;
  }

  void WifiConnectScreen::setSSID(const std::string &ssid)
  {
    ssid_ = ssid;
  }

  void WifiConnectScreen::createScreen()
  {

    lv_obj_set_size(lvObj_, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(lvObj_);

    lv_obj_t *title = lv_label_create(lvObj_);
    lv_label_set_text_fmt(title, "Connect to: %s", ssid_.c_str());
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Create a vertical container for label + password row
    lv_obj_t *pwd_container = lv_obj_create(lvObj_);
    lv_obj_align(pwd_container, LV_ALIGN_TOP_LEFT, 10, 80);
    lv_obj_set_size(pwd_container, 300, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pwd_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(pwd_container, 4, 0);
    lv_obj_clear_flag(pwd_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(pwd_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pwd_container, 0, 0);
    lv_obj_set_style_outline_width(pwd_container, 0, 0);
    lv_obj_set_style_pad_all(pwd_container, 0, 0);

    // Password box
    lv_obj_t *pwd_label = lv_label_create(pwd_container);
    lv_label_set_text(pwd_label, "Password:");

    // Create a horizontal container for the password field + eye button
    lv_obj_t *pwd_row = lv_obj_create(pwd_container);
    lv_obj_set_size(pwd_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pwd_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(pwd_row, 8, 0);
    lv_obj_clear_flag(pwd_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(pwd_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pwd_row, 0, 0);
    lv_obj_set_style_outline_width(pwd_row, 0, 0);
    lv_obj_set_style_pad_all(pwd_row, 0, 0);

    // Create the password text area
    pwd_textarea_ = lv_textarea_create(pwd_row);
    lv_obj_set_flex_grow(pwd_textarea_, 1);
    lv_textarea_set_placeholder_text(pwd_textarea_, "Enter password");
    lv_textarea_set_password_mode(pwd_textarea_, true);
    lv_textarea_set_one_line(pwd_textarea_, true);
    lv_obj_add_event_cb(pwd_textarea_, textarea_event_handler, LV_EVENT_ALL, this);

    // pwd_textarea_ = lv_textarea_create(lvObj_);
    // lv_textarea_set_password_mode(pwd_textarea_, true);
    // lv_textarea_set_one_line(pwd_textarea_, true);
    // lv_obj_set_width(pwd_textarea_, LV_PCT(80));
    // lv_obj_align_to(pwd_textarea_, pwd_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // Create the show/hide button
    lv_obj_t *eye_btn = lv_btn_create(pwd_row);
    lv_obj_set_size(eye_btn, 40, 40);

    // Add an eye icon label (you could also use an image instead)
    lv_obj_t *eye_lbl = lv_label_create(eye_btn);
    lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(eye_lbl);

    lv_obj_add_flag(eye_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(eye_btn, [](lv_event_t *e)
                        {
        lv_obj_t * btn = lv_event_get_target(e);
        bool checked = lv_obj_has_state(btn, LV_STATE_CHECKED);
        lv_obj_t * row = lv_obj_get_parent(btn);
        lv_obj_t * ta = lv_obj_get_child(row, 0);
        lv_obj_t * lbl = lv_obj_get_child(btn, 0);
        lv_textarea_set_password_mode(ta, !checked);
        lv_label_set_text(lbl, checked ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN); }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Keyboard (hidden by default)
    lv_obj_t *layer_top = lv_layer_top(); // LVGL’s dedicated overlay layer
    keyboard_ = lv_keyboard_create(layer_top);
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(keyboard_, LV_HOR_RES, LV_VER_RES / 3);
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard_, keyboard_event_cb, LV_EVENT_ALL, this);

    // Buttons
    connect_btn_ = lv_btn_create(lvObj_);
    lv_obj_set_size(connect_btn_, 100, 40);
    lv_obj_align(connect_btn_, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
    lv_obj_t *connect_label = lv_label_create(connect_btn_);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);
    lv_obj_add_event_cb(connect_btn_, connect_btn_event_handler, LV_EVENT_CLICKED, this);

    cancel_btn_ = lv_btn_create(lvObj_);
    lv_obj_set_size(cancel_btn_, 100, 40);
    lv_obj_align(cancel_btn_, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn_);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
    lv_obj_add_event_cb(cancel_btn_, cancel_btn_event_handler, LV_EVENT_CLICKED, this);

    status_label_ = lv_label_create(lv_scr_act());
    lv_label_set_text(status_label_, "");
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);

    spinner_ = lv_spinner_create(lv_scr_act(), 1000, 60);
    lv_obj_center(spinner_);
    lv_obj_set_size(spinner_, 60, 60);
    lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
  }

  void WifiConnectScreen::wifi_connect_task(void *pvParameter)
  {
    WifiConnectScreen::wifi_credentials_t *creds = (WifiConnectScreen::wifi_credentials_t *)pvParameter;

    ESP_LOGI("WIFI_CONNECT", "Connecting to SSID: %s", creds->ssid);

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));

    // Start connection
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK)
    {
      ESP_LOGE("WIFI_CONNECT", "esp_wifi_connect failed: %s", esp_err_to_name(err));
      lv_msg_send(MSG_WIFI_FAILED, NULL);
      vTaskDelete(NULL);
      return;
    }

    // Wait for result via events
    // Optional: timeout (e.g., 10s)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
    {
      lv_msg_send(MSG_WIFI_CONNECTED, NULL);
    }
    else
    {
      lv_msg_send(MSG_WIFI_FAILED, NULL);
    }

    vTaskDelete(NULL);
  }

  void WifiConnectScreen::onConnectPressed()
  {
    lv_label_set_text(status_label_, "Connecting...");
    lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(spinner_, LV_OBJ_FLAG_HIDDEN);

    const char *password = lv_textarea_get_text(pwd_textarea_);

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid_.c_str());

    creds = std::make_shared<wifi_credentials_t>();
    strncpy(creds->ssid, ssid_.c_str(), sizeof(creds->ssid));
    strncpy(creds->password, password, sizeof(creds->password));

    xTaskCreate(&wifi_connect_task, "wifi_connect_task", 4096, creds.get(), 5, nullptr);
  }

  void WifiConnectScreen::connect_btn_event_handler(lv_event_t *e)
  {
    auto self = static_cast<WifiConnectScreen *>(lv_event_get_user_data(e));
    self->onConnectPressed();
  }

  void WifiConnectScreen::textarea_event_handler(lv_event_t *e)
  {

    auto self = static_cast<WifiConnectScreen *>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED)
    {
      // Show keyboard when textarea focused
      // lv_obj_add_flag(self->connect_btn_, LV_OBJ_FLAG_HIDDEN);
      // lv_obj_add_flag(self->cancel_btn_, LV_OBJ_FLAG_HIDDEN);

      lv_obj_clear_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);
      lv_keyboard_set_textarea(self->keyboard_, self->pwd_textarea_);
    }
    else if (code == LV_EVENT_DEFOCUSED)
    {
      // Hide keyboard when focus lost
      lv_obj_add_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN);

      // lv_obj_clear_flag(self->connect_btn_, LV_OBJ_FLAG_HIDDEN);
      // lv_obj_clear_flag(self->cancel_btn_, LV_OBJ_FLAG_HIDDEN);

      lv_keyboard_set_textarea(self->keyboard_, nullptr);
    }
  }

  void WifiConnectScreen::cancel_btn_event_handler(lv_event_t *e)
  {
    auto self = static_cast<WifiConnectScreen *>(lv_event_get_user_data(e));
    // Handle cancel action
    ESP_LOGI(TAG, "Connection cancelled for SSID: %s", self->ssid_.c_str());
    if (auto screen = self->parentScreen_.lock())
    {
      screen->show();
    }
  }

  void WifiConnectScreen::keyboard_event_cb(lv_event_t *e)
  {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);

    if (code == LV_EVENT_READY)
    {
      // OK / Enter pressed
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
      if (ta)
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
      // Trigger your connect button or handler here
      // wifi_connect_start();
    }
    else if (code == LV_EVENT_CANCEL)
    {
      // Close button pressed (×)
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
      if (ta)
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    }
  }
} // namespace display