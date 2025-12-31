#include "ConnectDCC.h"
#include "DCCMenu.h"
#include "LvglWrapper.h"
#include "Screen.h"
#include "WaitingScreen.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include "FirstScreen.h"
#include <memory>
#include <vector>

namespace display {

static const char *TAG = "DCC_CONNECT_SCREEN";

void ConnectDCCScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {

  detectedListItems.clear();
  currentButton = nullptr;
  savedListItem = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Connect DCC", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_auto = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_auto, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // List for Saved
  // list_saved = makeListView(tab_saved, 0, 0, LV_PCT(100), lv_obj_get_height(tab_saved));

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &ConnectDCCScreen::event_back_trampoline, LV_EVENT_CLICKED, this);
  btn_save = makeButton(lvObj_, "Save", 100, 40, LV_ALIGN_BOTTOM_MID, 0, -12, "button.primary");
  lv_obj_add_event_cb(btn_save, &ConnectDCCScreen::event_save_trampoline, LV_EVENT_CLICKED, this);
  btn_connect = makeButton(lvObj_, "Connect", 100, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.primary");
  lv_obj_add_event_cb(btn_connect, &ConnectDCCScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  mdns_added_sub = lv_msg_subscribe(
      MSG_MDNS_DEVICE_ADDED,
      [](void *s, lv_msg_t *msg) {
        ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(msg->user_data);
        self->refreshMdnsList();
      },
      this);
  mdns_changed_sub = lv_msg_subscribe(
      MSG_MDNS_DEVICE_CHANGED,
      [](void *s, lv_msg_t *msg) {
        ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(msg->user_data);
        self->refreshMdnsList();
      },
      this);

  subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, [](void *s, lv_msg_t *msg) {
    ESP_LOGI(TAG, "Not connected to wifi");
    auto firstScreen = FirstScreen::instance();
    firstScreen->showScreen();
  },this);
  refreshMdnsList();
}

void ConnectDCCScreen::refreshMdnsList() {
  auto wifiHandler = utilities::WifiHandler::instance();
  auto devices = wifiHandler->getWithrottleDevices();
  ESP_LOGI(TAG, "Refreshing mDNS list, found %d devices", devices.size());

  for (const auto &device : devices) {
    bool found = false;
    for (auto &item : detectedListItems) {
      if (item->matches(device)) // Assume DCCConnectListItem has a matches() method for identity
      {
        found = true;
        if (!item->isSame(device)) // Assume isSame() checks if details differ
        {
          item->update(device); // Update the item with new device info
          ESP_LOGI(TAG, "Updated device: %s", device.instance.c_str());
        }
        break;
      }
    }
    if (!found) {
      ESP_LOGI(TAG, "Adding new device: %s", device.instance.c_str());
      auto listItem = std::make_shared<DCCConnectListItem>(list_auto, detectedListItems.size(), device);
      detectedListItems.push_back(listItem);
    }
  }
}

void ConnectDCCScreen::resetMsgHandlers() {
  if (subscribe_failed) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
  if (mdns_added_sub) {
    lv_msg_unsubscribe(mdns_added_sub);
    mdns_added_sub = nullptr;
  }
  if (mdns_changed_sub) {
    lv_msg_unsubscribe(mdns_changed_sub);
    mdns_changed_sub = nullptr;
  }
}

void ConnectDCCScreen::refreshSavedList() {
  // for (const auto &item : savedListItems)
  // {
  //     // Assume item is already added to list_saved_ during save action
  //     // If additional refresh logic is needed, implement here
  // }
}

void ConnectDCCScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up ConnectDCCScreen");
  resetMsgHandlers();
  detectedListItems.clear();
  currentButton = nullptr;
  savedListItem = nullptr;
  lv_obj_del(lvObj_);
}

void ConnectDCCScreen::connectToDCCServer(std::shared_ptr<DCCConnectListItem> dccItem) {
  if (!dccItem) {
    ESP_LOGW(TAG, "No DCC item selected for connection");
    return;
  }

  ESP_LOGI(TAG, "Connecting to DCC server at %s:%d", dccItem->device().ip.c_str(), dccItem->device().port);

  auto dccDevice = dccItem->device();
  auto wifiHandler = utilities::WifiHandler::instance();
  auto wifiControl = utilities::WifiControl::instance();

  auto waitingScreen = std::make_shared<WaitingScreen>();
  waitingScreen->setLabel("Connecting to:");
  waitingScreen->setSubLabel(dccItem->device().hostname);
  waitingScreen->showScreen(shared_from_this());

  lv_msg_send(MSG_CONNECTING_TO_DCC_SERVER, NULL);
  wifiControl->startConnectToServer(dccDevice.ip.c_str(), dccDevice.port);

  // Create a FreeRTOS task to wait for connection on core 0
  struct ConnectTaskArgs {
    std::shared_ptr<utilities::WifiHandler> wifiHandler;
    std::shared_ptr<utilities::WifiControl> wifiControl;
    std::string ip;
    int port;
    std::string instance;
  };
  auto *args = new ConnectTaskArgs{wifiHandler, wifiControl, dccDevice.ip, dccDevice.port, dccDevice.instance};

  auto connect_wait_task = [](void *arg) {
    auto *args = static_cast<ConnectTaskArgs *>(arg);
    utilities::WifiControl::connection_state currentConnectionState = utilities::WifiControl::NOT_CONNECTED;

    do {
      vTaskDelay(pdMS_TO_TICKS(50));
      currentConnectionState = args->wifiControl->connectionState();
    } while (currentConnectionState == utilities::WifiControl::CONNECTING);

    if (currentConnectionState == utilities::WifiControl::CONNECTED) {
      args->wifiHandler->stopMdnsSearchLoop();
      lv_msg_send(MSG_DCC_CONNECTION_SUCCESS, NULL);
      auto DCCMenuScreen = DCCMenu::instance();
      ESP_LOGI(TAG, "Successfully connected to DCC server at %s:%d", args->ip.c_str(), args->port);
      DCCMenuScreen->setConnectedServer(args->ip, args->port, args->instance);
      DCCMenuScreen->showScreen();
    } else {
      lv_msg_send(MSG_DCC_CONNECTION_FAILED, NULL);
      ESP_LOGI(TAG, "Failed to connect to DCC server at %s:%d", args->ip.c_str(), args->port);
    }
    delete args;
    vTaskDelete(nullptr);
  };

  xTaskCreatePinnedToCore(connect_wait_task, "connect_wait_task", 4096, args, 5, nullptr,
                          0 // Core 0
  );
}

void ConnectDCCScreen::button_connect_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    resetMsgHandlers();
    // TODO: Connect to selected DCC connection
    if (currentButton) {
      auto currentItem = getItem(currentButton);
      if (currentItem) {
        ESP_LOGI(TAG, "Connect button pressed on %s", currentItem->getText().c_str());
        connectToDCCServer(currentItem);
      }
    }
  }
}

void ConnectDCCScreen::button_save_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (currentButton) {
      auto currentItem = getItem(currentButton);
      if (currentItem) {
        ESP_LOGI(TAG, "Save button pressed on %s", currentItem->getText().c_str());
        // handle save action.
        // auto savedListItem = std::make_shared<DCCConnectListItem>(list_saved, savedListItems.size(),
        // dccItem->device()); savedListItems.push_back(savedListItem);
      }
    }
  }
}

void ConnectDCCScreen::button_back_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      resetMsgHandlers();
      screen->showScreen();
    }
  }
}

void ConnectDCCScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI("CONNECT_DCC", "List item clicked");

    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    // You can handle the list item click event here if needed
    lv_obj_t *target = lv_event_get_target(e);
    /*The current target is always the container as the event is added to it*/
    lv_obj_t *cont = lv_event_get_current_target(e);

    /*If container was clicked do nothing*/
    if (target == cont) {
      ESP_LOGI("CONNECT_DCC", "Clicked on container, ignoring");
      return;
    }

    // void * event_data = lv_event_get_user_data(e);
    ESP_LOGI("CONNECT_DCC", "Clicked: %s", lv_list_get_btn_text(list_auto, target));

    if (currentButton == target) {
      currentButton = NULL;
    } else {
      currentButton = target;
    }
    lv_obj_t *parent = lv_obj_get_parent(target);
    uint32_t i;
    for (i = 0; i < lv_obj_get_child_cnt(parent); i++) {
      lv_obj_t *child = lv_obj_get_child(parent, i);
      if (child == currentButton) {
        ESP_LOGI("CONNECT_DCC", "Setting CHECKED state on %s", lv_list_get_btn_text(list_auto, child));
        lv_obj_add_state(child, LV_STATE_CHECKED);
        lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
      } else {
        ESP_LOGI("CONNECT_DCC", "Clearing CHECKED state on %s", lv_list_get_btn_text(list_auto, child));
        lv_obj_clear_state(child, LV_STATE_CHECKED);
      }
    }
  }
}

std::shared_ptr<DCCConnectListItem> ConnectDCCScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : detectedListItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display