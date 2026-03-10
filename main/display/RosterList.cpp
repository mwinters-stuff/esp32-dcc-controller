#include "RosterList.h"
#include "DCCMenu.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "Screen.h"
#include "WaitingScreen.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "utilities/WifiHandler.h"
#include <memory>
#include <vector>

namespace display {

static const char *TAG = "ROSTER_LIST_SCREEN";

void RosterListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;

  listItems.clear();
  currentButton = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Roster", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_roster = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_roster, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &RosterListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);
  // btn_save = makeButton(lvObj_, "Save", 100, 40, LV_ALIGN_BOTTOM_MID, 0, -12, "button.primary");
  // lv_obj_add_event_cb(btn_save, &RosterListScreen::event_save_trampoline, LV_EVENT_CLICKED, this);
  // btn_connect = makeButton(lvObj_, "Connect", 100, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.primary");
  // lv_obj_add_event_cb(btn_connect, &RosterListScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  subscribe_failed = lv_msg_subscribe(
      MSG_WIFI_FAILED,
      [](lv_msg_t *msg) {
        RosterListScreen *self = static_cast<RosterListScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "Not connected to wifi");
        self->cleanUp();
        auto firstScreen = FirstScreen::instance();
        firstScreen->showScreen();
      },
      this);

  refreshList();
}

void RosterListScreen::refreshList() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh roster list");
    return;
  }

  if (dccProtocol->receivedRoster() == false) {
    ESP_LOGW(TAG, "DCC Protocol has not received roster list, cannot refresh roster list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_roster);
  auto loco = DCCExController::Loco::getFirst();
  while (loco != nullptr) {
    if(loco->getSource() == DCCExController::LocoSource::LocoSourceRoster) {
      const char *name = loco->getName();
      if(name != nullptr && strlen(name) > 0) {
      ESP_LOGI(TAG, "Roster ID=%d, Name=%s", loco->getAddress(), name);

      auto listItem = std::make_shared<RosterListItem>(list_roster, listItems.size(), loco->getAddress(), name);
      listItems.push_back(listItem);
      } else {
        ESP_LOGI(TAG, "Roster ID=%d has no name, skipping", loco->getAddress());
      }
    } else {
      ESP_LOGI(TAG, "Skipping non-roster loco with ID=%d", loco->getAddress());
    }
    loco = loco->getNext();
  }
}

void RosterListScreen::unsubscribeAll() {
  if (subscribe_failed) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
}

void RosterListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up RosterListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_roster = nullptr;
  btn_back = nullptr;
  currentButton = nullptr;
  lv_obj_clean(lvObj_);
}

void RosterListScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

void RosterListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "List item clicked");

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
    ESP_LOGI(TAG, "Clicked: %s", lv_list_get_btn_text(list_roster, target));

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
        ESP_LOGI(TAG, "Setting CHECKED state on %s", lv_list_get_btn_text(list_roster, child));
        lv_obj_add_state(child, LV_STATE_CHECKED);
        // lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
      } else {
        ESP_LOGI(TAG, "Clearing CHECKED state on %s", lv_list_get_btn_text(list_roster, child));
        lv_obj_clear_state(child, LV_STATE_CHECKED);
      }
    }
  }
}

std::shared_ptr<RosterListItem> RosterListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display