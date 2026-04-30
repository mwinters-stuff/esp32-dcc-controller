/**
 * @file TurnoutList.cpp
 * @brief Screen displaying the turnout/point list from the DCC-EX server.
 *
 * Each entry shows the turnout name and thrown/closed state. Tapping or
 * pressing the rotary encoder toggles the turnout via WiThrottle. Supports
 * rotary-encoder navigation with focus outlines.
 */
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

// Builds the turnout list UI, registers rotary callbacks and subscribes to
// turnout updated / thrown-state messages.
void TurnoutListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;

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

  turnout_changed_sub = lv_msg_subscribe(
      MSG_DCC_TURNOUT_CHANGED,
      [](lv_msg_t *msg) {
        TurnoutListScreen *self = static_cast<TurnoutListScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "Turnout changed message received");
        TurnoutActionData *data = static_cast<TurnoutActionData *>(const_cast<void *>(lv_msg_get_payload(msg)));
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
  rotaryAttach();
}

// Clears and repopulates the list widget from the latest turnout data.
void TurnoutListScreen::refreshList() {
  ESP_LOGI(TAG, "Refreshing turnout list");
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

  focusedIndex = listItems.empty() ? -1 : 0;
  updateFocusedState();
}

// Removes turnout-related lv_msg subscriptions.
void TurnoutListScreen::unsubscribeAll() {
  if (turnout_changed_sub) {
    lv_msg_unsubscribe(turnout_changed_sub);
    turnout_changed_sub = nullptr;
  }
}

// Releases widget pointers, unregisters rotary callbacks and clears item list.
void TurnoutListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up TurnoutListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_turnouts = nullptr;
  btn_back = nullptr;
  currentButton = nullptr;
  focusedIndex = -1;
  rotaryDetach();
  lv_obj_clean(lvObj_);
}

// Returns to the previous screen.
void TurnoutListScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

// Handles touch taps on list items: toggles the turnout thrown state.
void TurnoutListScreen::button_listitem_click_event_callback(lv_event_t *e) {
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
    ESP_LOGI(TAG, "Toggling: %s", lv_list_get_btn_text(list_turnouts, target));

    auto item = getItem(target);
    if (item) {
      focusedIndex = static_cast<int>(item->index);
      updateFocusedState();
      throwTurnout(item, !item->isThrown());
    } else {
      ESP_LOGW(TAG, "No item found for clicked button");
    }
  }
}

// Refreshes focus outlines across all list items.
void TurnoutListScreen::updateFocusedState() {
  for (size_t i = 0; i < listItems.size(); ++i) {
    auto obj = listItems[i]->getLvObj();
    if (!obj) {
      continue;
    }
    if (static_cast<int>(i) == focusedIndex) {
      lv_obj_add_state(obj, LV_STATE_FOCUSED);
      applyFocusOutline(obj, true);
      lv_obj_scroll_to_view(obj, LV_ANIM_OFF);
    } else {
      lv_obj_clear_state(obj, LV_STATE_FOCUSED);
      applyFocusOutline(obj, false);
    }
  }
}

// Moves the rotary focus by `direction` steps (+1 down, -1 up).
void TurnoutListScreen::moveFocus(int direction) {
  if (isCleanedUp || listItems.empty() || direction == 0) {
    return;
  }

  int index = focusedIndex;
  if (index < 0 || index >= static_cast<int>(listItems.size())) {
    index = 0;
  }

  index = (index + direction) % static_cast<int>(listItems.size());
  if (index < 0) {
    index += static_cast<int>(listItems.size());
  }

  focusedIndex = index;
  updateFocusedState();
}

// Sends a throw/close command for the focused turnout.
void TurnoutListScreen::activateFocused() {
  if (isCleanedUp || focusedIndex < 0 || focusedIndex >= static_cast<int>(listItems.size())) {
    return;
  }

  auto item = listItems[focusedIndex];
  if (item) {
    throwTurnout(item, !item->isThrown());
  }
}

// Sends the throw/close command for `item` with the specified new state.
void TurnoutListScreen::throwTurnout(std::shared_ptr<TurnoutListItem> item, bool newThrownState) {
  ESP_LOGI(TAG, "Found item for turnout ID %d new thrown %s", item->getTurnoutId(),
           newThrownState ? "thrown" : "closed");
  auto wifiControl = utilities::WifiControl::instance();
  auto turnout = DCCExController::Turnout::getById(item->getTurnoutId());
  if (!turnout) {
    ESP_LOGW(TAG, "No turnout found for ID %d", item->index);
    return;
  }

  ESP_LOGI(TAG, "Found turnout for turnout ID %d cuurrent thrown %s", turnout->getId(),
           turnout->getThrown() ? "thrown" : "closed");
  if (newThrownState == turnout->getThrown()) {
    ESP_LOGI(TAG, "Turnout ID %d is already in the desired state %s, update display", turnout->getId(),
             newThrownState ? "thrown" : "closed");
    item->updateThrown(newThrownState);
    return;
  }

  ESP_LOGI(TAG, "Setting turnout ID %d to %s", turnout->getId(), newThrownState ? "thrown" : "closed");
  if (wifiControl->setTurnoutThrown(turnout->getId(), newThrownState)) {
    // Optimistically update display; server updates still reconcile via delegate messages.
    item->updateThrown(newThrownState);
  } else {
    ESP_LOGW(TAG, "Unable to send turnout command while disconnected");
  }
}

// Returns the TurnoutListItem whose LVGL button matches bn, or nullptr.
std::shared_ptr<TurnoutListItem> TurnoutListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

// Returns the TurnoutListItem matching the given DCC turnout ID, or nullptr.
std::shared_ptr<TurnoutListItem> TurnoutListScreen::getItemByTurnoutId(int turnoutId) {
  for (const auto &item : listItems) {
    if (item->getTurnoutId() == turnoutId) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display