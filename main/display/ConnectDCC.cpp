/**
 * @file ConnectDCC.cpp
 * @brief Screen for discovering and connecting to a DCC-EX server via mDNS.
 *
 * Lists WiThrottle-compatible servers found on the local network. Supports
 * saving a preferred server to NVS and auto-connecting to it on boot. Once a
 * connection is established the screen transitions to DCCMenu.
 */
#include "ConnectDCC.h"
#include "DCCMenu.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "MessageBox.h"
#include "Screen.h"
#include "WaitingScreen.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <memory>
#include <nvs_handle.hpp>
#include <vector>

namespace display {

static const char *TAG = "DCC_CONNECT_SCREEN";
bool ConnectDCCScreen::bootAutoConnectHandled = false;

namespace {

void stop_mdns_search_async() {
  auto *handler = utilities::WifiHandler::instance().get();
  BaseType_t taskCreated = xTaskCreate(
      [](void *arg) {
        auto *wifiHandler = static_cast<utilities::WifiHandler *>(arg);
        wifiHandler->stopMdnsSearchLoop();
        vTaskDelete(nullptr);
      },
      "mdns_stop", 3072, handler, tskIDLE_PRIORITY + 1, nullptr);
  if (taskCreated != pdPASS) {
    ESP_LOGW(TAG, "Failed to create mdns_stop task; stopping mDNS inline");
    handler->stopMdnsSearchLoop();
  }
}

} // namespace

// Returns true if the one-shot boot auto-connect attempt has already been made.
bool ConnectDCCScreen::isBootAutoConnectHandled() { return bootAutoConnectHandled; }

// Marks the boot auto-connect attempt as consumed so it is not retried.
void ConnectDCCScreen::markBootAutoConnectHandled() { bootAutoConnectHandled = true; }

// Builds the UI, starts (or resumes) mDNS discovery and, on first boot,
// shows the discovered DCC servers.
void ConnectDCCScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;
  autoConnectAttempted = false;
  lv_obj_clean(lv_screen_active());
  lvObj_ = lv_screen_active();

  detectedListItems.clear();
  currentButton = nullptr;
  savedListItem = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Connect DCC", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_auto = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_auto, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &ConnectDCCScreen::event_back_trampoline, LV_EVENT_CLICKED, this);
  btn_save = makeButton(lvObj_, "Save", 100, 40, LV_ALIGN_BOTTOM_MID, 0, -12, "button.primary");
  lv_obj_add_event_cb(btn_save, &ConnectDCCScreen::event_save_trampoline, LV_EVENT_CLICKED, this);
  btn_connect = makeButton(lvObj_, "Connect", 100, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.primary");
  lv_obj_add_event_cb(btn_connect, &ConnectDCCScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  mdns_added_sub = lv_msg_subscribe(
      MSG_MDNS_DEVICE_ADDED,
      [](lv_msg_t *msg) {
        ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(lv_msg_get_user_data(msg));
        self->refreshMdnsList();
      },
      this);
  mdns_changed_sub = lv_msg_subscribe(
      MSG_MDNS_DEVICE_CHANGED,
      [](lv_msg_t *msg) {
        ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(lv_msg_get_user_data(msg));
        self->refreshMdnsList();
      },
      this);

  connect_success = lv_msg_subscribe(
      MSG_DCC_CONNECTION_SUCCESS,
      [](lv_msg_t *msg) {
        ESP_LOGI(TAG, "Connected to DCC server successfully");
        // ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(lv_msg_get_user_data(msg));
      },
      this);

  wifi_connected_sub = lv_msg_subscribe(
      MSG_WIFI_CONNECTED,
      [](lv_msg_t *msg) {
        ConnectDCCScreen *self = static_cast<ConnectDCCScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        auto wifiHandler = utilities::WifiHandler::instance();
        if (wifiHandler->isConnected()) {
          wifiHandler->startMdnsSearchLoop();
        }
      },
      this);

  refreshMdnsList();
  auto wifiHandler = utilities::WifiHandler::instance();
  if (wifiHandler->isConnected()) {
    wifiHandler->startMdnsSearchLoop();
  } else {
    ESP_LOGW(TAG, "WiFi is not connected; skipping mDNS search loop start");
  }

  focusedIndex = -1;
  rotaryAttach();
}

// Reads the saved DCC server details from NVS into outDevice.
// Returns false if no valid entry has been saved.
bool ConnectDCCScreen::loadSavedConnection(utilities::WithrottleDevice &outDevice) {
  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_DCC, NVS_READONLY, &err);
  if (err != ESP_OK || !handle) {
    return false;
  }

  uint8_t saved = 0;
  err = handle->get_item(NVS_DCC_SAVED, saved);
  if (err != ESP_OK || saved == 0) {
    return false;
  }

  char ip[16] = {0};
  uint16_t port = 0;
  err = handle->get_string(NVS_DCC_IP, ip, sizeof(ip));
  if (err != ESP_OK || ip[0] == '\0') {
    return false;
  }

  err = handle->get_item(NVS_DCC_PORT, port);
  if (err != ESP_OK || port == 0) {
    return false;
  }

  char instance[64] = {0};
  char hostname[64] = {0};
  if (handle->get_string(NVS_DCC_INSTANCE, instance, sizeof(instance)) != ESP_OK) {
    instance[0] = '\0';
  }
  if (handle->get_string(NVS_DCC_HOSTNAME, hostname, sizeof(hostname)) != ESP_OK) {
    hostname[0] = '\0';
  }

  outDevice.ip = ip;
  outDevice.port = port;
  outDevice.instance = instance;
  outDevice.hostname = hostname;
  return true;
}

// Synchronises the on-screen list with the current set of discovered devices.
// Always prepends the saved entry (if any) so it is always visible even before
// mDNS finds it. Existing items are updated in place; new ones are appended.
void ConnectDCCScreen::refreshMdnsList() {
  if (isCleanedUp)
    return;

  // Guard against stale callbacks after this screen's widgets have been deleted.
  if (!lvObj_ || !lv_obj_is_valid(lvObj_) || !list_auto || !lv_obj_is_valid(list_auto) || lv_screen_active() != lvObj_)
    return;

  auto wifiHandler = utilities::WifiHandler::instance();
  auto devices = wifiHandler->getWithrottleDevices();

  // Always include the saved DCC entry in this list. If mDNS later discovers the
  // same endpoint, DCCConnectListItem::matches() prevents duplicates.
  utilities::WithrottleDevice savedDevice;
  const bool hasSavedDevice = loadSavedConnection(savedDevice);
  if (hasSavedDevice) {
    devices.insert(devices.begin(), savedDevice);
  }

  // Preserve the currently selected endpoint (if any) across a list rebuild.
  std::string selectedIp;
  uint16_t selectedPort = 0;
  if (currentButton) {
    auto selectedItem = getItem(currentButton);
    if (selectedItem) {
      selectedIp = selectedItem->device().ip;
      selectedPort = selectedItem->device().port;
    }
  }

  // Dedupe by endpoint (ip+port), preserving order so a saved entry inserted at
  // the front remains first.
  std::vector<utilities::WithrottleDevice> uniqueDevices;
  uniqueDevices.reserve(devices.size());
  for (const auto &device : devices) {
    bool duplicate = false;
    for (const auto &existing : uniqueDevices) {
      if (existing.ip == device.ip && existing.port == device.port) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      uniqueDevices.push_back(device);
    }
  }

  ESP_LOGI(TAG, "Refreshing mDNS list, found %d devices", uniqueDevices.size());

  detectedListItems.clear();
  lv_obj_clean(list_auto);
  currentButton = nullptr;

  for (const auto &device : uniqueDevices) {
    const bool isSaved = hasSavedDevice && savedDevice.ip == device.ip && savedDevice.port == device.port;
    auto listItem = std::make_shared<DCCConnectListItem>(list_auto, detectedListItems.size(), device, isSaved);
    detectedListItems.push_back(listItem);

    if (!selectedIp.empty() && device.ip == selectedIp && device.port == selectedPort) {
      currentButton = listItem->getLvObj();
    }
  }

  if (currentButton) {
    lv_obj_add_state(currentButton, LV_STATE_CHECKED);
  }

  if (btn_connect) {
    if (currentButton) {
      lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    }
  }

  const int totalFocusable = static_cast<int>(detectedListItems.size()) + 3;
  if (focusedIndex < 0 || focusedIndex >= totalFocusable) {
    focusedIndex = 0;
  }

  updateFocusedState();
}

// Unsubscribes all lv_msg subscriptions created during show(). Called when
// navigating away or before starting a connection attempt.
void ConnectDCCScreen::resetMsgHandlers() {
  if (mdns_added_sub) {
    lv_msg_unsubscribe(mdns_added_sub);
    mdns_added_sub = nullptr;
  }
  if (mdns_changed_sub) {
    lv_msg_unsubscribe(mdns_changed_sub);
    mdns_changed_sub = nullptr;
  }
  if (connect_success) {
    lv_msg_unsubscribe(connect_success);
    connect_success = nullptr;
  }
  if (wifi_connected_sub) {
    lv_msg_unsubscribe(wifi_connected_sub);
    wifi_connected_sub = nullptr;
  }
}

// Tears down the screen: unsubscribes messages, clears widget pointers and
// releases the waiting-screen handle.
void ConnectDCCScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up ConnectDCCScreen");
  isCleanedUp = true;
  rotaryDetach();
  stop_mdns_search_async();
  resetMsgHandlers();
  detectedListItems.clear();

  lv_obj_clean(lvObj_);

  lbl_title = nullptr;
  list_auto = nullptr;
  btn_back = nullptr;
  btn_save = nullptr;
  btn_connect = nullptr;
  currentButton = nullptr;
  savedListItem = nullptr;
  focusedIndex = -1;
  waitingScreen_.reset();
}

// Convenience overload: extracts the WithrottleDevice from a list item and
// delegates to connectToDCCDevice.
void ConnectDCCScreen::connectToDCCServer(std::shared_ptr<DCCConnectListItem> dccItem) {
  if (!dccItem) {
    ESP_LOGW(TAG, "No DCC item selected for connection");
    return;
  }

  connectToDCCDevice(dccItem->device());
}

// Stops mDNS updates, shows the waiting screen and spawns a FreeRTOS task that
// waits for the TCP connection to resolve. On success it transitions to DCCMenu;
// on failure it fires MSG_DCC_CONNECTION_FAILED and dismisses the waiting screen.
void ConnectDCCScreen::connectToDCCDevice(const utilities::WithrottleDevice &dccDevice) {
  if (dccDevice.ip.empty() || dccDevice.port == 0) {
    ESP_LOGW(TAG, "Invalid DCC device details, cannot connect");
    return;
  }

  ESP_LOGI(TAG, "Connecting to DCC server at %s:%d", dccDevice.ip.c_str(), dccDevice.port);

  auto wifiHandler = utilities::WifiHandler::instance();
  auto wifiControl = utilities::WifiControl::instance();

  // Stop mDNS once we leave the discovery screen and before the waiting screen
  // takes over.
  stop_mdns_search_async();

  // Stop receiving mDNS list updates while the waiting screen is active.
  resetMsgHandlers();

  waitingScreen_ = std::make_shared<WaitingScreen>();
  waitingScreen_->setLabel("Connecting to:");
  waitingScreen_->setSubLabel(dccDevice.hostname.empty() ? dccDevice.ip : dccDevice.hostname);
  waitingScreen_->showScreen(shared_from_this());

  lv_msg_send(MSG_CONNECTING_TO_DCC_SERVER, NULL);
  wifiControl->startConnectToServer(dccDevice.ip.c_str(), dccDevice.port);

  // Create a FreeRTOS task to wait for connection on core 0
  struct ConnectTaskArgs {
    std::shared_ptr<ConnectDCCScreen> self;
    std::shared_ptr<utilities::WifiHandler> wifiHandler;
    std::shared_ptr<utilities::WifiControl> wifiControl;
    std::string ip;
    int port;
    std::string instance;
  };
  auto *args = new ConnectTaskArgs{shared_from_this(), wifiHandler,    wifiControl,
                                   dccDevice.ip,       dccDevice.port, dccDevice.instance};

  auto connect_wait_task = [](void *arg) {
    auto *args = static_cast<ConnectTaskArgs *>(arg);
    utilities::WifiControl::connection_state currentConnectionState = utilities::WifiControl::NOT_CONNECTED;

    do {
      vTaskDelay(pdMS_TO_TICKS(50));
      currentConnectionState = args->wifiControl->connectionState();
    } while (currentConnectionState == utilities::WifiControl::CONNECTING);

    if (currentConnectionState == utilities::WifiControl::CONNECTED) {
      ESP_LOGI(TAG, "Successfully connected to DCC server at %s:%d", args->ip.c_str(), args->port);

      // All LVGL calls must happen on the LVGL task — schedule via lv_async_call.
      // Ownership of args transfers to the callback; do NOT delete here.
      lv_async_call(
          [](void *cbArg) {
            auto *args = static_cast<ConnectTaskArgs *>(cbArg);
            lv_msg_send(MSG_DCC_CONNECTION_SUCCESS, NULL);
            args->self->waitingScreen_.reset();
            args->self->cleanUp();
            auto DCCMenuScreen = DCCMenu::instance();
            DCCMenuScreen->setConnectedServer(args->ip, args->port, args->instance);
            DCCMenuScreen->showScreen();
            delete args;
          },
          args);
    } else {
      ESP_LOGI(TAG, "Failed to connect to DCC server at %s:%d", args->ip.c_str(), args->port);

      lv_async_call(
          [](void *cbArg) {
            auto *args = static_cast<ConnectTaskArgs *>(cbArg);
            lv_msg_send(MSG_DCC_CONNECTION_FAILED, NULL);
            args->self->waitingScreen_.reset();
            delete args;
          },
          args);
    }
    vTaskDelete(nullptr);
  };

  xTaskCreate(connect_wait_task, "connect_wait_task", 4096, args, tskIDLE_PRIORITY, nullptr);
}

// Initiates a connection to the currently selected list item.
void ConnectDCCScreen::button_connect_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (currentButton) {
      auto currentItem = getItem(currentButton);
      if (currentItem) {
        ESP_LOGI(TAG, "Connect button pressed on %s", currentItem->getText().c_str());
        connectToDCCServer(currentItem);
      }
    }
  }
}

// Persists the selected server to NVS and shows a confirmation message box.
void ConnectDCCScreen::button_save_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (currentButton) {
      auto currentItem = getItem(currentButton);
      if (currentItem) {
        ESP_LOGI(TAG, "Save button pressed on %s", currentItem->getText().c_str());
        if (saveSelectedConnection()) {
          display::showMessageBox("Saved", "Saved DCC connection.", display::MessageBoxState::Success, nullptr,
                                  nullptr);
        } else {
          display::showMessageBox("Save Failed", "Unable to save DCC connection.", display::MessageBoxState::Error,
                                  nullptr, nullptr);
        }
      }
    }
  }
}

// Writes the selected device's IP, port, instance and hostname to NVS.
// Returns true on success.
bool ConnectDCCScreen::saveSelectedConnection() {
  if (!currentButton) {
    ESP_LOGW(TAG, "No selected DCC item to save");
    return false;
  }

  auto currentItem = getItem(currentButton);
  if (!currentItem) {
    ESP_LOGW(TAG, "Selected DCC item not found");
    return false;
  }

  const auto &device = currentItem->device();
  if (device.ip.empty() || device.port == 0) {
    ESP_LOGW(TAG, "Selected DCC item missing ip/port");
    return false;
  }

  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_DCC, NVS_READWRITE, &err);
  if (err != ESP_OK || !handle) {
    ESP_LOGE(TAG, "nvs_open for DCC save failed: %s", esp_err_to_name(err));
    return false;
  }

  err = handle->set_item(NVS_DCC_SAVED, static_cast<uint8_t>(1));
  if (err == ESP_OK)
    err = handle->set_string(NVS_DCC_INSTANCE, device.instance.c_str());
  if (err == ESP_OK)
    err = handle->set_string(NVS_DCC_HOSTNAME, device.hostname.c_str());
  if (err == ESP_OK)
    err = handle->set_string(NVS_DCC_IP, device.ip.c_str());
  if (err == ESP_OK)
    err = handle->set_item(NVS_DCC_PORT, device.port);
  if (err == ESP_OK)
    err = handle->commit();

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save DCC connection: %s", esp_err_to_name(err));
    return false;
  }

  savedListItem = currentItem;
  ESP_LOGI(TAG, "Saved DCC connection %s:%d", device.ip.c_str(), device.port);

  // Rebuild list so saved-icon state updates immediately.
  refreshMdnsList();

  return true;
}

// Performs the one-shot boot auto-connect for the main-menu flow: if WiFi is
// up, no connection is already in progress and a saved server exists,
// connects immediately. Returns true if a connection attempt was started.
bool ConnectDCCScreen::maybeAutoConnectSaved() {
  if (isCleanedUp || autoConnectAttempted) {
    return false;
  }

  if (isBootAutoConnectHandled()) {
    return false;
  }

  if (!utilities::WifiHandler::instance()->isConnected()) {
    ESP_LOGI(TAG, "Skipping DCC auto-connect: WiFi is not connected");
    return false;
  }

  // Auto-connect is boot-only. Once WiFi is up and we evaluate this path, consume the boot attempt.
  markBootAutoConnectHandled();

  utilities::WithrottleDevice savedDevice;
  if (!loadSavedConnection(savedDevice)) {
    return false;
  }

  auto wifiControl = utilities::WifiControl::instance();
  if (wifiControl->connectionState() == utilities::WifiControl::CONNECTING ||
      wifiControl->connectionState() == utilities::WifiControl::CONNECTED) {
    ESP_LOGI(TAG, "Skipping DCC auto-connect: DCC already connecting/connected");
    autoConnectAttempted = true;
    return false;
  }

  ESP_LOGI(TAG, "Auto-connecting to saved DCC server %s:%d", savedDevice.ip.c_str(), savedDevice.port);
  autoConnectAttempted = true;
  connectToDCCDevice(savedDevice);
  return true;
}

// Returns to the parent screen (typically FirstScreen).
void ConnectDCCScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

// Handles taps on individual list entries: toggles the checked state and
// enables or disables the Connect button accordingly.
void ConnectDCCScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (isCleanedUp || !btn_connect)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "List item clicked");

    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    // You can handle the list item click event here if needed
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    /*The current target is always the container as the event is added to it*/
    lv_obj_t *cont = (lv_obj_t *)lv_event_get_current_target(e);

    /*If container was clicked do nothing*/
    if (target == cont) {
      ESP_LOGI(TAG, "Clicked on container, ignoring");
      return;
    }

    // void * event_data = lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Clicked: %s", lv_list_get_btn_text(list_auto, target));

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
        ESP_LOGI(TAG, "Setting CHECKED state on %s", lv_list_get_btn_text(list_auto, child));
        lv_obj_add_state(child, LV_STATE_CHECKED);
        lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
      } else {
        ESP_LOGI(TAG, "Clearing CHECKED state on %s", lv_list_get_btn_text(list_auto, child));
        lv_obj_clear_state(child, LV_STATE_CHECKED);
      }
    }
  }
}

// Moves rotary focus by `direction` steps through list items and the three
// bottom buttons (Back, Save, Connect).
void ConnectDCCScreen::moveFocus(int direction) {
  if (isCleanedUp || direction == 0) {
    return;
  }

  const int total = static_cast<int>(detectedListItems.size()) + 3; // Back, Save, Connect
  if (total == 3 && detectedListItems.empty()) {
    // No list items yet — still allow cycling through buttons
  }

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

// Applies focus outlines to the focused item/button; clears all others.
// Does NOT change CHECKED/selection state — that is owned by rotaryActivateFocused
// and the touch click handler.
void ConnectDCCScreen::updateFocusedState() {
  const int listSize = static_cast<int>(detectedListItems.size());

  for (int i = 0; i < listSize; ++i) {
    auto obj = detectedListItems[i]->getLvObj();
    if (!obj) {
      continue;
    }
    applyFocusOutline(obj, i == focusedIndex);
    if (i == focusedIndex) {
      lv_obj_scroll_to_view(obj, LV_ANIM_OFF);
    }
  }

  applyFocusOutline(btn_back, focusedIndex == listSize);
  applyFocusOutline(btn_save, focusedIndex == listSize + 1);
  applyFocusOutline(btn_connect, focusedIndex == listSize + 2);
}

void ConnectDCCScreen::rotaryMoveFocus(int direction) { moveFocus(direction); }

// Single click: select the focused list item (mirrors touch-tap selection), or
// fire the focused button.
void ConnectDCCScreen::rotaryActivateFocused() {
  if (isCleanedUp || focusedIndex < 0) {
    return;
  }

  const int listSize = static_cast<int>(detectedListItems.size());

  if (focusedIndex < listSize) {
    // Toggle selection on the focused list item
    auto obj = detectedListItems[focusedIndex]->getLvObj();
    if (!obj) {
      return;
    }
    if (currentButton == obj) {
      currentButton = nullptr;
    } else {
      currentButton = obj;
    }
    for (int i = 0; i < listSize; ++i) {
      auto itemObj = detectedListItems[i]->getLvObj();
      if (!itemObj) {
        continue;
      }
      if (itemObj == currentButton) {
        lv_obj_add_state(itemObj, LV_STATE_CHECKED);
      } else {
        lv_obj_clear_state(itemObj, LV_STATE_CHECKED);
      }
    }
    if (btn_connect) {
      if (currentButton) {
        lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
      } else {
        lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
      }
    }
  } else if (focusedIndex == listSize && btn_back) {
    lv_obj_send_event(btn_back, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == listSize + 1 && btn_save) {
    lv_obj_send_event(btn_save, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == listSize + 2 && btn_connect) {
    lv_obj_send_event(btn_connect, LV_EVENT_CLICKED, nullptr);
  }
}

// On double-click: if the focused item matches the saved connection, prompt to
// remove it. Otherwise, silently ignore.
void ConnectDCCScreen::rotaryHandleDoubleClick() {
  if (isCleanedUp) {
    return;
  }

  utilities::WithrottleDevice savedDevice;
  if (!loadSavedConnection(savedDevice)) {
    return;
  }

  // Determine which item to check: focused (rotary) or touch-selected.
  std::shared_ptr<DCCConnectListItem> target;
  if (focusedIndex >= 0 && focusedIndex < static_cast<int>(detectedListItems.size())) {
    target = detectedListItems[focusedIndex];
  } else if (currentButton) {
    target = getItem(currentButton);
  }

  if (!target) {
    return;
  }

  const auto &dev = target->device();
  if (dev.ip != savedDevice.ip || dev.port != savedDevice.port) {
    return;
  }

  display::showMessageBox(
      "Remove Saved",
      ("Remove saved DCC connection?\n" + dev.instance + " (" + dev.ip + ":" + std::to_string(dev.port) + ")").c_str(),
      display::MessageBoxState::Warning,
      [](void *ctx) {
        auto *self = static_cast<ConnectDCCScreen *>(ctx);
        if (self) {
          self->removeSavedConnection();
        }
      },
      this);
}

// Clears the saved DCC entry from NVS and refreshes the list display.
void ConnectDCCScreen::removeSavedConnection() {
  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_DCC, NVS_READWRITE, &err);
  if (err != ESP_OK || !handle) {
    ESP_LOGE(TAG, "nvs_open for DCC remove failed: %s", esp_err_to_name(err));
    return;
  }
  handle->erase_item(NVS_DCC_SAVED);
  handle->erase_item(NVS_DCC_IP);
  handle->erase_item(NVS_DCC_PORT);
  handle->erase_item(NVS_DCC_INSTANCE);
  handle->erase_item(NVS_DCC_HOSTNAME);
  handle->commit();
  ESP_LOGI(TAG, "Saved DCC connection removed");
  savedListItem = nullptr;
  currentButton = nullptr;
  focusedIndex = -1;
  refreshMdnsList();
  updateFocusedState();
  display::showMessageBox("Removed", "Saved DCC connection removed.", display::MessageBoxState::Success, nullptr,
                          nullptr);
}

// Returns the DCCConnectListItem whose LVGL button matches bn, or nullptr.
std::shared_ptr<DCCConnectListItem> ConnectDCCScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : detectedListItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display