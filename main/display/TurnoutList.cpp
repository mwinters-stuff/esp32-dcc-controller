#include "TurnoutList.h"
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

static const char *TAG = "TURNOUT_LIST_SCREEN";

void TurnoutListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {

  listItems.clear();
  currentButton = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Turnouts", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_turnouts = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_turnouts, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &TurnoutListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  subscribe_failed = lv_msg_subscribe(
      MSG_WIFI_FAILED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "Not connected to wifi");
        auto firstScreen = FirstScreen::instance();
        firstScreen->showScreen();
      },
      this);

  turnout_changed_sub = lv_msg_subscribe(
      MSG_DCC_TURNOUT_CHANGED,
      [](void *s, lv_msg_t *msg) {
        ESP_LOGI(TAG, "Turnout changed message received");
        auto self = static_cast<TurnoutListScreen *>(msg->user_data);
        TurnoutActionData *data = static_cast<TurnoutActionData *>(const_cast<void *>(msg->payload));
        ESP_LOGI(TAG, "Turnout ID=%d changed to Thrown=%s", data->turnoutId, data->thrown ? "thrown" : "closed");
        auto item = self->getItemByTurnoutId(data->turnoutId);
        if (item) {
          self->throwTurnout(item, data->thrown);
        } else {
          ESP_LOGW(TAG, "No item found for turnout ID %d", data->turnoutId);
        }
      },
      this);

  refreshList();
}

void TurnoutListScreen::refreshList() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh turnout list");
    return;
  }

  if (dccProtocol->receivedTurnoutList() == false) {
    ESP_LOGW(TAG, "DCC Protocol has not received turnout list, cannot refresh turnout list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_turnouts);

  for (auto turnout = DCCExController::Turnout::getFirst(); turnout; turnout = turnout->getNext()) {
    ESP_LOGI(TAG, "Turnout ID=%d, Name=%s, Thrown=%s", turnout->getId(), turnout->getName(),
             turnout->getThrown() ? "thrown" : "closed");

    auto listItem = std::make_shared<TurnoutListItem>(list_turnouts, listItems.size(), turnout->getId(),
                                                      turnout->getName(), turnout->getThrown());
    listItems.push_back(listItem);
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
    ESP_LOGI(TAG, "Toggling: %s", lv_list_get_btn_text(list_turnouts, target));

    auto item = getItem(target);
    if (item) {
      throwTurnout(item, !item->isThrown());
    } else {
      ESP_LOGW(TAG, "No item found for clicked button");
    }
  }
}

void TurnoutListScreen::throwTurnout(std::shared_ptr<TurnoutListItem> item, bool newThrownState) {
  ESP_LOGI(TAG, "Found item for turnout ID %d new thrown %s", item->getTurnoutId(),
           newThrownState ? "thrown" : "closed");
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();
  if (dccProtocol) {
    auto turnout = DCCExController::Turnout::getById(item->getTurnoutId());
    if (turnout) {
      ESP_LOGI(TAG, "Found turnout for turnout ID %d cuurrent thrown %s", turnout->getId(),
               turnout->getThrown() ? "thrown" : "closed");
      if (newThrownState == turnout->getThrown()) {
        ESP_LOGI(TAG, "Turnout ID %d is already in the desired state %s, update display", turnout->getId(),
                 newThrownState ? "thrown" : "closed");
        item->updateThrown(newThrownState);
        return;
      }
      ESP_LOGI(TAG, "Setting turnout ID %d to %s", turnout->getId(), newThrownState ? "thrown" : "closed");
      if (newThrownState) {
        dccProtocol->throwTurnout(turnout->getId());
      } else {
        dccProtocol->closeTurnout(turnout->getId());
      }
      // Update the list item display
      item->updateThrown(newThrownState);

    } else {
      ESP_LOGW(TAG, "No turnout found for ID %d", item->index);
    }
  } else {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot toggle turnout");
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

std::shared_ptr<TurnoutListItem> TurnoutListScreen::getItemByTurnoutId(int turnoutId) {
  for (const auto &item : listItems) {
    if (item->getTurnoutId() == turnoutId) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display