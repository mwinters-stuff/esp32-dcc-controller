/**
 * @file WifiListScreen.cpp
 * @brief Screen that scans for nearby WiFi networks and lets the user pick one.
 *
 * On entry, stops any running mDNS search and kicks off a background scan task.
 * When the scan completes the list is populated and the user can select an
 * access point and tap Connect to proceed to WifiConnectScreen.
 */
#include "WifiListScreen.h"
#include "LvglWrapper.h"
#include "WifiConnectScreen.h"
#include "WifiListItem.h"
#include "definitions.h"
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <regex.h>
#include <string>

namespace display {

static const char *TAG = "WIFI_LIST_SCREEN";

// Builds the WiFi scan UI, stops mDNS and starts the background scan task.
void WifiListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  utilities::WifiHandler::instance()->stopMdnsSearchLoop();
  isCleanedUp = false;

  // Full-screen container
  lv_obj_set_size(lvObj_, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
  lv_obj_clear_flag(lvObj_, LV_OBJ_FLAG_SCROLLABLE);

  // Title label at top
  lbl_title = makeLabel(lvObj_, "WiFi List", LV_ALIGN_TOP_MID, 0, 8, "label.title");

  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, event_back_trampoline, LV_EVENT_CLICKED, this);

  btn_connect = makeButton(lvObj_, "Connect", 100, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.primary");
  lv_obj_add_event_cb(btn_connect, event_connect_trampoline, LV_EVENT_CLICKED, this);

  // List view below
  list_view = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_view, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  ESP_LOGI("WIFI_LIST_SCREEN", "WifiListScreen shown");

  items.clear();

  // Create spinner overlay (centered)
  spinner = makeSpinner(lvObj_, 0, 0, 50);

  disableButtons();

  // Start Wi-Fi scan in a separate task
  xTaskCreate(
      [](void *param) {
        auto *self = static_cast<WifiListScreen *>(param);
        self->scanWifiTask();
        self->scanTaskHandle = 0; // null before self-delete so cleanUp() won't double-delete
        vTaskDelete(nullptr);
      },
      "wifi_scan_task",
      4096, // stack size
      this,
      tskIDLE_PRIORITY, // priority
      &scanTaskHandle);
}

// Releases widget pointers and aborts any running scan task.
void WifiListScreen::cleanUp() {
  ESP_LOGI(TAG, "WifiListScreen cleaned up");
  isCleanedUp = true;
  if (scanTaskHandle != 0) {
    vTaskDelete(scanTaskHandle);
    scanTaskHandle = 0;
  }
  lbl_title = nullptr;
  btn_back = nullptr;
  btn_connect = nullptr;
  list_view = nullptr;
  spinner = nullptr;
  currentButton = nullptr;
  lv_obj_clean(lvObj_);
}

// FreeRTOS task body: runs the esp_wifi scan, then uses lv_async_call to
// populate the list on the LVGL thread.
void WifiListScreen::scanWifiTask() {
  ESP_LOGI(TAG, "Starting background Wi-Fi scan...");
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_scan_config_t scan_conf = {};
  scan_conf.ssid = nullptr;
  scan_conf.bssid = nullptr;
  scan_conf.channel = 0;
  scan_conf.show_hidden = true;

  ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_conf, true));

  uint16_t ap_count = 0;
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  if (ap_count == 0) {
    ESP_LOGI(TAG, "No APs found");
    lv_async_call(
        [](void *data) {
          auto *self = static_cast<WifiListScreen *>(data);
          if (self->isCleanedUp)
            return;
          lv_obj_add_flag(self->spinner, LV_OBJ_FLAG_HIDDEN);
        },
        this);
    return;
  }

  uint16_t max_records = std::min<uint16_t>(ap_count, DEFAULT_SCAN_LIST_SIZE);
  std::vector<wifi_ap_record_t> ap_info(max_records);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_records, ap_info.data()));

  // Copy results into heap before passing to LVGL thread
  auto *results = new std::vector<wifi_ap_record_t>(ap_info);

  // Now update UI safely
  lv_async_call(
      [](void *data) {
        auto *ctx = static_cast<std::pair<WifiListScreen *, std::vector<wifi_ap_record_t> *> *>(data);
        if (ctx->first->isCleanedUp) {
          delete ctx->second;
          delete ctx;
          return;
        }
        auto *self = ctx->first;
        auto *records = ctx->second;
        self->populateList(*records);
        lv_obj_add_flag(self->spinner, LV_OBJ_FLAG_HIDDEN);
        self->enableButtons();
        delete records;
        delete ctx;
      },
      new std::pair<WifiListScreen *, std::vector<wifi_ap_record_t> *>(this, results));

  ESP_ERROR_CHECK(esp_wifi_scan_stop());
}

// Populates the list widget with the scan results.
void WifiListScreen::populateList(const std::vector<wifi_ap_record_t> &records) {
  for (const auto &ap : records) {
    std::string ssid_str(reinterpret_cast<const char *>(ap.ssid),
                         strnlen(reinterpret_cast<const char *>(ap.ssid), sizeof(ap.ssid)));
    if (ssid_str.empty())
      ssid_str = "<hidden>";
    ESP_LOGI(TAG, "Found AP: SSID='%s', RSSI=%d", ssid_str.c_str(), ap.rssi);

    auto item = std::make_shared<WifiListItem>(list_view, items.size(), ssid_str, ap.rssi);
    items.push_back(item);
  }

  ESP_LOGI(TAG, "Wi-Fi scan complete, list populated with %u entries", (unsigned)records.size());
}

// Disables the Connect and Back buttons (used during scan).
void WifiListScreen::disableButtons() {
  if (isCleanedUp)
    return;
  ESP_LOGI(TAG, "DisableButtons");
  lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
  lv_obj_add_state(btn_back, LV_STATE_DISABLED);
}

// Re-enables the Connect and Back buttons after the scan completes.
void WifiListScreen::enableButtons() {
  if (isCleanedUp)
    return;
  ESP_LOGI(TAG, "EnableButtons");
  lv_obj_clear_state(btn_back, LV_STATE_DISABLED);
}

// Validates that a network is selected and opens WifiConnectScreen.
void WifiListScreen::button_connect_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI("WIFI_LIST_SCREEN", "Connect button pressed");
    auto instance = WifiConnectScreen::instance();
    auto currentItem = getCurrentCheckedItem(currentButton);
    if (currentItem) {
      // Now you can access WifiListItem-specific methods
      cleanUp();
      instance->setSSID(currentItem->getSsid());
      instance->showScreen(WifiListScreen::instance());
    }
  }
}

// Returns the currently checked WifiListItem, or nullptr if none selected.
std::shared_ptr<WifiListItem> WifiListScreen::getCurrentCheckedItem(lv_obj_t *bn) {
  for (const auto &item : items) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

// Returns to the previous screen.
void WifiListScreen::button_back_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI("WIFI_LIST_SCREEN", "Back button pressed");
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

// Handles taps on list items: checks the tapped entry and unchecks others.
void WifiListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI("WIFI_LIST_SCREEN", "List item clicked");

    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    // You can handle the list item click event here if needed
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    /*The current target is always the container as the event is added to it*/
    lv_obj_t *cont = (lv_obj_t *)lv_event_get_current_target(e);

    /*If container was clicked do nothing*/
    if (target == cont) {
      ESP_LOGI("WIFI_LIST_SCREEN", "Clicked on container, ignoring");
      return;
    }

    // void * event_data = lv_event_get_user_data(e);
    ESP_LOGI("WIFI_LIST_SCREEN", "Clicked: %s", lv_list_get_btn_text(list_view, target));

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
        ESP_LOGI("WIFI_LIST_SCREEN", "Setting CHECKED state on %s", lv_list_get_btn_text(list_view, child));
        lv_obj_add_state(child, LV_STATE_CHECKED);
        lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
      } else {
        ESP_LOGI("WIFI_LIST_SCREEN", "Clearing CHECKED state on %s", lv_list_get_btn_text(list_view, child));
        lv_obj_clear_state(child, LV_STATE_CHECKED);
      }
    }
  }
}

} // namespace display