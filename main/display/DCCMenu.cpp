
#include "DCCMenu.h"
#include "ConnectDCC.h"
#include "FirstScreen.h"
#include "definitions.h"
#include "connection/wifi_control.h"
#include <esp_log.h>

namespace display {
static const char *TAG = "DCC_MENU_SCREEN";

// static void wifi_connected_cb(void *s, lv_msg_t *msg) {
//   DCCMenu *self = static_cast<DCCMenu *>(msg->user_data);
//   self->enableButtons(true);
//   ESP_LOGI(TAG, "Connected to wifi");
//   if (self->lbl_status)
//     self->lbl_status->setText("WiFi Connected");
//   if (self->lbl_ip) {
//     if (auto ip = utilities::WifiHandler::instance()->getIpAddress(); !ip.empty())
//       self->lbl_ip->setText(ip);
//   }
// }

static void wifi_failed_cb(void *s, lv_msg_t *msg) {
  // DCCMenu *self = static_cast<DCCMenu*>(msg->user_data);
  // self->enableButtons(false);
  ESP_LOGI(TAG, "Not connected to wifi");
  auto firstScreen = FirstScreen::instance();
  firstScreen->showScreen();
  // if (self->lbl_status)
  //   self->lbl_status->setText("WiFi Disconnected");
  // if (self->lbl_ip)
  //   self->lbl_ip->setText("");
}

void DCCMenu::setConnectedServer(std::string ip, int port, std::string dccname) {
  this->ip = ip;
  this->port = port;
  this->dccname = dccname;
}

void DCCMenu::enableButtons(bool enableConnect) {
  ESP_LOGI(TAG, "EnableButtons");
  btn_roster->setEnabled(true);
  btn_points->setEnabled(true);
  btn_routes->setEnabled(true);
  btn_turntables->setEnabled(true);
}

void DCCMenu::disableButtons() {
  ESP_LOGI(TAG, "DisableButtons");
  btn_roster->setEnabled(false);
  btn_points->setEnabled(false);
  btn_routes->setEnabled(false);
  btn_turntables->setEnabled(false);

}

void DCCMenu::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  // Title Label
  lbl_title =
      std::make_shared<ui::LvglLabel>(lvObj_, dccname, LV_ALIGN_TOP_MID, 0, 10, &lv_font_montserrat_30);
  lbl_title->setStyle("label.title");

  // "Roster" button
  btn_roster = std::make_shared<ui::LvglButton>(
      lvObj_, "Roster",
      [this](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          ESP_LOGI(TAG, "Roster button clicked!");
          unsubscribeAll();
          // auto connectDCCScreen = ConnectDCCScreen::instance();
          // connectDCCScreen->showScreen(DCCMenu::instance());
        }
      },
      200, 48, LV_ALIGN_CENTER, 0, -120);
  btn_roster->setStyle("button.primary");

  // "Points" button
  btn_points = std::make_shared<ui::LvglButton>(
      lvObj_, "Points",
      [this](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          ESP_LOGI(TAG, "Points button clicked!");
          unsubscribeAll();
            // auto wifiScreen = WifiListScreen::instance();
            // wifiScreen->showScreen(DCCMenu::instance());
        }
      },
      200, 48, LV_ALIGN_CENTER, 0, -60);
  btn_points->setStyle("button.primary");

  // "Routes" button
  btn_routes = std::make_shared<ui::LvglButton>(
      lvObj_, "Routes",
      [this](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          ESP_LOGI(TAG, "Routes button clicked!");
          unsubscribeAll();
        }
      },
      200, 48, LV_ALIGN_CENTER, 0, 0);
  btn_routes->setStyle("button.primary");
  // "Turntables" button
  btn_turntables = std::make_shared<ui::LvglButton>(
      lvObj_, "Turntables",
      [this](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          ESP_LOGI(TAG, "Turntables button clicked!");
          unsubscribeAll();
        }
      },
      200, 48, LV_ALIGN_CENTER, 0, 60);
  btn_turntables->setStyle("button.primary");
  
  // "Close" button
  btn_close = std::make_shared<ui::LvglButton>(
      lvObj_, "Close",
      [this](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
          ESP_LOGI(TAG, "Close button clicked!");
          unsubscribeAll();
          disableButtons();
          auto wifiControl = WifiControl::instance();
          wifiControl->disconnect();
          auto firstScreen = FirstScreen::instance();
          firstScreen->showScreen();

        }
      },
      200, 48, LV_ALIGN_CENTER, 0, 180);

  btn_close->setStyle("button.secondary");
  disableButtons();

  auto wifiHandler = utilities::WifiHandler::instance();

  subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, wifi_failed_cb, this);
  subscribe_not_saved = lv_msg_subscribe(MSG_WIFI_NOT_SAVED, wifi_failed_cb, this);

  subscribe_dcc_roster_received = lv_msg_subscribe(
      MSG_DCC_ROSTER_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Roster list received message");
        // Handle roster list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        self->lbl_status->setText("Roster list received.");
        self->btn_roster->setEnabled(true);
      },
      this);
  subscribe_dcc_turnout_received = lv_msg_subscribe(
      MSG_DCC_TURNOUT_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Turnout list received message");
        // Handle turnout list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        self->lbl_status->setText("Turnout list received.");
        self->btn_points->setEnabled(true);
      },
      this);

  subscribe_dcc_route_received = lv_msg_subscribe(
      MSG_DCC_ROUTE_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Route list received message");
        // Handle route list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        self->lbl_status->setText("Route list received.");
        self->btn_routes->setEnabled(true);
      },
      this);
  subscribe_dcc_turntable_received = lv_msg_subscribe(
      MSG_DCC_TURNTABLE_LIST_RECEIVED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "DCC Turntable list received message");
        // Handle turntable list received
        std::shared_ptr<DCCMenu> self = static_cast<DCCMenu *>(msg->user_data)->shared_from_this();
        self->lbl_status->setText("Turntable list received.");
        self->btn_turntables->setEnabled(true);
      },
      this);

  // Status labels under the last button
  // Show connection state and IP (IP empty when disconnected)
  lbl_status = std::make_shared<ui::LvglLabel>(lvObj_, "",
                                               LV_ALIGN_TOP_MID, 0, 40);
  lbl_status->setStyle("label.main");

  // std::string ip_text;
  // if (connected) {
  //   // assumes WifiHandler exposes a string-returning accessor; adapt name if different
  //   ip_text = wifiHandler->getIpAddress();
  // } else {
  //   disableButtons();
  // }
  // lbl_ip = std::make_shared<ui::LvglLabel>(lvObj_, ip_text, LV_ALIGN_CENTER, 0, 150);
  // lbl_ip->setStyle("label.muted");

  ESP_LOGI(TAG, "DCCMenu UI created");
}

void DCCMenu::unsubscribeAll() {
  if (subscribe_failed != nullptr) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
  if (subscribe_not_saved != nullptr) {
    lv_msg_unsubscribe(subscribe_not_saved);
    subscribe_not_saved = nullptr;
  }

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
}

void DCCMenu::cleanUp() {
  ESP_LOGI(TAG, "DCCMenu cleaned up");
  // lbl_title.reset();
  // btn_connect.reset();
  // btn_wifi_scan.reset();
  // btn_cal.reset();
  // lbl_status.reset();
  // lbl_ip.reset();
  // Screen::cleanUp();
}

} // namespace display
