#include "FirstScreen.h"
#include "ManualCalibration.h"
#include "WifiListScreen.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>

namespace display
{
  static const char *TAG = "FIRST_SCREEN";


  void* FirstScreen::subscribe_connected = nullptr;
  void* FirstScreen::subscribe_failed = nullptr;
  void* FirstScreen::subscribe_not_saved = nullptr;

  static void wifi_connected_cb(void *s, lv_msg_t *msg)
  {
    FirstScreen *self = static_cast<FirstScreen *>(msg->user_data);
    self->enableButtons(true);
    ESP_LOGI(TAG, "Connected to wifi");
    if (self->lbl_status)
      self->lbl_status->setText("WiFi Connected");
    if (self->lbl_ip)
    {
      if (auto ip = utilities::WifiHandler::instance()->getIpAddress(); !ip.empty())
        self->lbl_ip->setText(ip);
    }
  }

  static void wifi_failed_cb(void *s, lv_msg_t *msg)
  {
    FirstScreen *self = static_cast<FirstScreen *>(msg->user_data);
    self->enableButtons(false);
    ESP_LOGI(TAG, "Not connected to wifi");
    if (self->lbl_status)
      self->lbl_status->setText("WiFi Disconnected");
    if (self->lbl_ip)
      self->lbl_ip->setText("");
  }

  void FirstScreen::enableButtons(bool enableConnect)
  {
    ESP_LOGI(TAG, "EnableButtons");
    btn_connect->setEnabled(enableConnect);
    btn_cal->setEnabled(true);
    btn_wifi_scan->setEnabled(true);
  }

  void FirstScreen::disableButtons()
  {
    ESP_LOGI(TAG, "DisableButtons");
    btn_connect->setEnabled(false);
    btn_cal->setEnabled(false);
    btn_wifi_scan->setEnabled(false);
  }

  void FirstScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen)
  {
    Screen::show(parent, parentScreen); // Ensure base setup (sets lvObj_, applies theme, etc.)

    // Title Label
    lbl_title = std::make_shared<ui::LvglLabel>(
        lvObj_, "DCC Controller", LV_ALIGN_TOP_MID, 0, 10, &lv_font_montserrat_30);
    lbl_title->setStyle("label.title");

    // "Connect" button
    btn_connect = std::make_shared<ui::LvglButton>(
        lvObj_, "Connect",
        [this](lv_event_t *e)
        {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED)
            ESP_LOGI(TAG, "Connect button clicked!");
        },
        200, 48, LV_ALIGN_CENTER, 0, -60);
    btn_connect->setStyle("button.primary");

    // "Scan WiFi" button
    btn_wifi_scan = std::make_shared<ui::LvglButton>(
        lvObj_, "Scan WiFi",
        [this](lv_event_t *e)
        {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED)
          {
            ESP_LOGI(TAG, "Scan WiFi button clicked!");
            auto wifiScreen = WifiListScreen::instance();
            wifiScreen->show(nullptr, FirstScreen::instance());
          }
        },
        200, 48, LV_ALIGN_CENTER, 0, 0);
    btn_wifi_scan->setStyle("button.primary");

    // "Calibrate" button
    btn_cal = std::make_shared<ui::LvglButton>(
        lvObj_, "Calibrate",
        [this](lv_event_t *e)
        {
          if (lv_event_get_code(e) == LV_EVENT_CLICKED)
          {
            ESP_LOGI(TAG, "Calibrate button clicked!");
            auto calScreen = ManualCalibration::instance();
            calScreen->show(nullptr, FirstScreen::instance());
          }
        },
        200, 48, LV_ALIGN_CENTER, 0, 60);

    btn_cal->setStyle("button.secondary");

    auto wifiHandler = utilities::WifiHandler::instance();

    if(subscribe_connected != nullptr){
      ESP_LOGI(TAG, "Unsubscribing");
      lv_msg_unsubscribe(subscribe_connected);
      lv_msg_unsubscribe(subscribe_failed);
      lv_msg_unsubscribe(subscribe_not_saved);
    }


    subscribe_connected = lv_msg_subscribe(MSG_WIFI_CONNECTED, wifi_connected_cb, this);
    subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, wifi_failed_cb, this);
    subscribe_not_saved = lv_msg_subscribe(MSG_WIFI_NOT_SAVED, wifi_failed_cb, this);

    // Status labels under the last button
    // Show connection state and IP (IP empty when disconnected)
    bool connected = wifiHandler->isConnected();
    lbl_status = std::make_shared<ui::LvglLabel>(
        lvObj_, connected ? "WiFi Connected" : "WiFi Disconnected",
        LV_ALIGN_CENTER, 0, 120);
    lbl_status->setStyle("label.main");

    std::string ip_text;
    if (connected)
    {
      // assumes WifiHandler exposes a string-returning accessor; adapt name if different
      ip_text = wifiHandler->getIpAddress();
    }
    else
    {
      disableButtons();
    }
    lbl_ip = std::make_shared<ui::LvglLabel>(
        lvObj_, ip_text, LV_ALIGN_CENTER, 0, 150);
    lbl_ip->setStyle("label.muted");

    ESP_LOGI(TAG, "FirstScreen UI created");
  }

  void FirstScreen::cleanUp()
  {
    lbl_title.reset();
    btn_connect.reset();
    btn_wifi_scan.reset();
    btn_cal.reset();
    lbl_status.reset();
    lbl_ip.reset();
    Screen::cleanUp();
  }

} // namespace display
