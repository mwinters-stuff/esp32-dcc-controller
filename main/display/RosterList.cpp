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
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "Not connected to wifi");
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
    ESP_LOGI(TAG, "Roster ID=%d, Name=%s", loco->getAddress(), loco->getName());

    auto listItem =
        std::make_shared<RosterListItem>(list_roster, listItems.size(), loco->getAddress(), loco->getName());
    listItems.push_back(listItem);
    loco = loco->getNext();
  }
}

void RosterListScreen::resetMsgHandlers() {
  if (subscribe_failed) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
}

void RosterListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up RosterListScreen");
  resetMsgHandlers();
  listItems.clear();
  currentButton = nullptr;
  lv_obj_del(lvObj_);
}

void RosterListScreen::button_back_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      resetMsgHandlers();
      screen->showScreen();
    }
  }
}

void RosterListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI(TAG, "List item clicked");

    // You can handle the list item click event here if needed
    lv_obj_t *target = lv_event_get_target(e);
    /*The current target is always the container as the event is added to it*/
    lv_obj_t *cont = lv_event_get_current_target(e);

    /*If container was clicked do nothing*/
    if (target == cont) {
      ESP_LOGI(TAG, "Clicked on container, ignoring");
      return;
    }

    // void * event_data = lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Toggling: %s", lv_list_get_btn_text(list_roster, target));

    // auto item = getItem(target);
    // if (item) {
    //   ESP_LOGI("CONNECT_DCC", "Found item for roster ID %d", item->getRosterId());
    //   auto wifiControl = utilities::WifiControl::instance();
    //   auto dccProtocol = wifiControl->dccProtocol();
    //   if(dccProtocol) {
    //     auto roster = DCCExController::Roster::getById(item->getRosterId());
    //     if(roster) {
    //       bool newThrownState = !roster->getThrown();
    //       ESP_LOGI("CONNECT_DCC", "Setting roster ID %d to %s", roster->getId(), newThrownState ? "thrown" :
    //       "closed"); if(newThrownState) {
    //         dccProtocol->throwRoster(roster->getId());
    //       } else {
    //         dccProtocol->closeRoster(roster->getId());
    //       }
    //       // Update the list item display
    //       item->updateName(newThrownState);

    //     } else {
    //       ESP_LOGW("CONNECT_DCC", "No roster found for ID %d", item->index);
    //     }
    //   } else {
    //     ESP_LOGW("CONNECT_DCC", "DCC Protocol is null, cannot toggle roster");
    //   }
    // } else {
    //   ESP_LOGW("CONNECT_DCC", "No item found for clicked button");
    // }
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