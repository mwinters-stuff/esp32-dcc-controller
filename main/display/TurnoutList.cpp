#include "TurnoutList.h"
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

static const char *TAG = "TURNOUT_LIST_SCREEN";

void TurnoutListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {

  listItems.clear();
  currentButton = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Turnouts", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_auto = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_auto, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // List for Saved
  // list_saved = makeListView(tab_saved, 0, 0, LV_PCT(100), lv_obj_get_height(tab_saved));

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &TurnoutListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);
  // btn_save = makeButton(lvObj_, "Save", 100, 40, LV_ALIGN_BOTTOM_MID, 0, -12, "button.primary");
  // lv_obj_add_event_cb(btn_save, &TurnoutListScreen::event_save_trampoline, LV_EVENT_CLICKED, this);
  // btn_connect = makeButton(lvObj_, "Connect", 100, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.primary");
  // lv_obj_add_event_cb(btn_connect, &TurnoutListScreen::event_connect_trampoline, LV_EVENT_CLICKED, this);

  subscribe_failed = lv_msg_subscribe(MSG_WIFI_FAILED, [](void *s, lv_msg_t *msg) {
    ESP_LOGI(TAG, "Not connected to wifi");
    auto firstScreen = FirstScreen::instance();
    firstScreen->showScreen();
  },this);

  refreshList();
}

void TurnoutListScreen::refreshList() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if(dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh turnout list");
    return;
  }

  if(dccProtocol->receivedTurnoutList() == false) {
    ESP_LOGW(TAG, "DCC Protocol has not received turnout list, cannot refresh turnout list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_auto);
  auto turnout = DCCExController::Turnout::getFirst();
  while(turnout != nullptr) {
    ESP_LOGI(TAG, "Turnout ID=%d, Name=%s, Thrown=%s", turnout->getId(), turnout->getName(), turnout->getThrown() ? "true" : "false");

    auto listItem = std::make_shared<TurnoutListItem>(list_auto, listItems.size(), turnout->getId(), turnout->getName(), turnout->getThrown());
    listItems.push_back(listItem);
    turnout = turnout->getNext();
  }
}

void TurnoutListScreen::resetMsgHandlers() {
  if (subscribe_failed) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
}

void TurnoutListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up TurnoutListScreen");
  resetMsgHandlers();
  listItems.clear();
  currentButton = nullptr;
  lv_obj_del(lvObj_);
}



void TurnoutListScreen::button_back_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      resetMsgHandlers();
      screen->showScreen();
    }
  }
}

void TurnoutListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    ESP_LOGI("CONNECT_DCC", "List item clicked");

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
    ESP_LOGI("CONNECT_DCC", "Toggling: %s", lv_list_get_btn_text(list_auto, target));

    auto item = getItem(target);
    if (item) {
      ESP_LOGI("CONNECT_DCC", "Found item for turnout ID %d", item->getTurnoutId());
      auto wifiControl = utilities::WifiControl::instance();
      auto dccProtocol = wifiControl->dccProtocol();
      if(dccProtocol) {
        auto turnout = DCCExController::Turnout::getById(item->getTurnoutId());
        if(turnout) {
          bool newThrownState = !turnout->getThrown();
          ESP_LOGI("CONNECT_DCC", "Setting turnout ID %d to %s", turnout->getId(), newThrownState ? "thrown" : "closed");
          if(newThrownState) {
            dccProtocol->throwTurnout(turnout->getId());
          } else {
            dccProtocol->closeTurnout(turnout->getId());
          }
          // Update the list item display
          item->updateName(newThrownState);
          
        } else {
          ESP_LOGW("CONNECT_DCC", "No turnout found for ID %d", item->index);
        }
      } else {
        ESP_LOGW("CONNECT_DCC", "DCC Protocol is null, cannot toggle turnout");
      }
    } else {
      ESP_LOGW("CONNECT_DCC", "No item found for clicked button");
    }
  }
}

std::shared_ptr<TurnoutListItem> TurnoutListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display