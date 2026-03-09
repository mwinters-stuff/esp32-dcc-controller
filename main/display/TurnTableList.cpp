#include "TurnTableList.h"
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

static const char *TAG = "TurnTable_LIST_SCREEN";

void TurnTableListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;

  listItems.clear();
  currentButton = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "TurnTables", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_TurnTables = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_TurnTables, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &TurnTableListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  subscribe_failed = lv_msg_subscribe(
      MSG_WIFI_FAILED,
      [](void *s, lv_msg_t *msg) {
        TurnTableListScreen *self = static_cast<TurnTableListScreen *>(msg->user_data);
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "Not connected to wifi");
        auto firstScreen = FirstScreen::instance();
        firstScreen->showScreen();
      },
      this);

  refreshList();
}

void TurnTableListScreen::refreshList() {
  ESP_LOGI(TAG, "Refreshing TurnTable list");
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh TurnTable list");
    return;
  }

  if (dccProtocol->receivedTurntableList() == false) {
    ESP_LOGW(TAG, "DCC Protocol has not received TurnTable list, cannot refresh TurnTable list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_TurnTables);

  for (auto TurnTable = DCCExController::Turntable::getFirst(); TurnTable; TurnTable = TurnTable->getNext()) {
    ESP_LOGI(TAG, "TurnTable ID=%d, Name=%s", TurnTable->getId(), TurnTable->getName());

    auto listItem = std::make_shared<TurnTableListItem>(list_TurnTables, listItems.size(), TurnTable->getId(),
                                                        TurnTable->getName());
    listItems.push_back(listItem);
  }
}

void TurnTableListScreen::unsubscribeAll() {
  if (subscribe_failed) {
    lv_msg_unsubscribe(subscribe_failed);
    subscribe_failed = nullptr;
  }
}

void TurnTableListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up TurnTableListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_TurnTables = nullptr;
  btn_back = nullptr;
  currentButton = nullptr;
  lv_obj_clean(lvObj_);
}

void TurnTableListScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

void TurnTableListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
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
    ESP_LOGI(TAG, "Moving To: %s", lv_list_get_btn_text(list_TurnTables, target));

    // auto item = getItem(target);
    // if (item) {
    //   throwTurnTable(item, !item->isThrown());
    // } else {
    //   ESP_LOGW(TAG, "No item found for clicked button");
    // }
  }
}

// void TurnTableListScreen::throwTurnTable(std::shared_ptr<TurnTableListItem> item, bool newThrownState) {
//   ESP_LOGI(TAG, "Found item for TurnTable ID %d new thrown %s", item->getTurnTableId(),
//            newThrownState ? "thrown" : "closed");
//   auto wifiControl = utilities::WifiControl::instance();
//   auto dccProtocol = wifiControl->dccProtocol();
//   if (dccProtocol) {
//     auto TurnTable = DCCExController::TurnTable::getById(item->getTurnTableId());
//     if (TurnTable) {
//       ESP_LOGI(TAG, "Found TurnTable for TurnTable ID %d cuurrent thrown %s", TurnTable->getId(),
//                TurnTable->getThrown() ? "thrown" : "closed");
//       if (newThrownState == TurnTable->getThrown()) {
//         ESP_LOGI(TAG, "TurnTable ID %d is already in the desired state %s, update display", TurnTable->getId(),
//                  newThrownState ? "thrown" : "closed");
//         item->updateThrown(newThrownState);
//         return;
//       }
//       ESP_LOGI(TAG, "Setting TurnTable ID %d to %s", TurnTable->getId(), newThrownState ? "thrown" : "closed");
//       if (newThrownState) {
//         dccProtocol->throwTurnTable(TurnTable->getId());
//       } else {
//         dccProtocol->closeTurnTable(TurnTable->getId());
//       }
//       // Update the list item display
//       item->updateThrown(newThrownState);

//     } else {
//       ESP_LOGW(TAG, "No TurnTable found for ID %d", item->index);
//     }
//   } else {
//     ESP_LOGW(TAG, "DCC Protocol is null, cannot toggle TurnTable");
//   }
// }

std::shared_ptr<TurnTableListItem> TurnTableListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

std::shared_ptr<TurnTableListItem> TurnTableListScreen::getItemByTurnTableId(int TurnTableId) {
  for (const auto &item : listItems) {
    if (item->getTurntableId() == TurnTableId) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display