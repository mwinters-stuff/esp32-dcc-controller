/**
 * @file RosterList.cpp
 * @brief Screen displaying the locomotive roster received from the DCC-EX server.
 *
 * Subscribes to MSG_ROSTER_UPDATED to keep the list in sync. Tapping a
 * locomotive opens DCCMenu for that engine. Supports rotary-encoder navigation
 * via the shared focus and selection helpers.
 */
#include "RosterList.h"
#include "LvglWrapper.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include <DCCEXLoco.h>
#include <cstdio>
#include <memory>
#include <vector>

namespace display {

static const char *TAG = "ROSTER_LIST_SCREEN";

// Builds the expandable roster list UI and subscribes to roster/loco updates.
void RosterListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  isCleanedUp = false;

  listItems.clear();
  focusedIndex = -1;
  expandedAddress = -1;

  // Title
  lbl_title = makeLabel(lvObj_, "Roster", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // Main roster list
  list_roster = makeListView(lvObj_, 0, 40, 320, 380);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &RosterListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  roster_received_sub = lv_msg_subscribe(
      MSG_DCC_ROSTER_LIST_RECEIVED,
      [](lv_msg_t *msg) {
        auto *self = static_cast<RosterListScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp) {
          return;
        }
        self->refreshList();
      },
      this);

  loco_changed_sub = lv_msg_subscribe(
      MSG_DCC_LOCO_CHANGED,
      [](lv_msg_t *msg) {
        auto *self = static_cast<RosterListScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp) {
          return;
        }

        auto *payload = static_cast<LocoStatePayload *>(const_cast<void *>(lv_msg_get_payload(msg)));
        if (!payload) {
          return;
        }

        self->applyLocoState(payload->address, payload->speed, static_cast<Direction>(payload->direction),
                             payload->functionMap);
      },
      this);

  refreshList();
  rotaryAttach();
}

// Clears and repopulates the list widget from the latest roster data.
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

  int previousExpandedAddress = expandedAddress;

  auto loco = Loco::getFirst();
  while (loco != nullptr) {
    if (loco->getSource() == LocoSource::LocoSourceRoster) {
      const char *name = loco->getName();
      ESP_LOGI(TAG, "Roster ID=%d, Name=%s", loco->getAddress(), (name != nullptr) ? name : "");

      auto listItem = std::make_shared<RosterListItem>(
          list_roster, listItems.size(), loco,
          [this](int address) {
            selectAddress(address);
          },
          [this](int address, int speed, Direction direction) {
            requestThrottle(address, speed, direction);
          },
          [this](int address) {
            requestStop(address);
          },
          [this](int address, int function, bool on) {
            requestFunction(address, function, on);
          });
      listItems.push_back(listItem);
    } else {
      ESP_LOGI(TAG, "Skipping non-roster loco with ID=%d", loco->getAddress());
    }
    loco = loco->getNext();
  }

  if (listItems.empty()) {
    focusedIndex = -1;
    expandedAddress = -1;
  } else {
    focusedIndex = 0;
    expandedAddress = -1;
    if (previousExpandedAddress >= 0 && getItemByAddress(previousExpandedAddress) != nullptr) {
      expandedAddress = previousExpandedAddress;
    }
  }

  updateExpandedState();
  updateFocusedState();
}

// Removes roster/loco message subscriptions.
void RosterListScreen::unsubscribeAll() {
  if (roster_received_sub != nullptr) {
    lv_msg_unsubscribe(roster_received_sub);
    roster_received_sub = nullptr;
  }

  if (loco_changed_sub != nullptr) {
    lv_msg_unsubscribe(loco_changed_sub);
    loco_changed_sub = nullptr;
  }
}

// Releases widget pointers and message subscriptions.
void RosterListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up RosterListScreen");
  isCleanedUp = true;
  rotaryDetach();
  unsubscribeAll();
  listItems.clear();
  focusedIndex = -1;
  expandedAddress = -1;
  lbl_title = nullptr;
  list_roster = nullptr;
  btn_back = nullptr;
  lv_obj_clean(lvObj_);
}

// Returns to the previous screen (typically FirstScreen).
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

void RosterListScreen::selectAddress(int address) {
  if (expandedAddress == address) {
    expandedAddress = -1;
  } else {
    expandedAddress = address;
  }

  for (size_t i = 0; i < listItems.size(); ++i) {
    if (listItems[i]->getAddress() == address) {
      focusedIndex = static_cast<int>(i);
      break;
    }
  }

  updateExpandedState();
  updateFocusedState();
}

void RosterListScreen::updateExpandedState() {
  for (const auto &item : listItems) {
    const bool expanded = expandedAddress >= 0 && item->getAddress() == expandedAddress;
    item->setExpanded(expanded);
    if (expanded) {
      lv_obj_scroll_to_view(item->getLvObj(), LV_ANIM_OFF);
    }
  }
}

// Returns the RosterListItem whose LVGL button matches bn, or nullptr.
std::shared_ptr<RosterListItem> RosterListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

std::shared_ptr<RosterListItem> RosterListScreen::getItemByAddress(int address) {
  for (const auto &item : listItems) {
    if (item->getAddress() == address) {
      return item;
    }
  }
  return nullptr;
}

void RosterListScreen::updateFocusedState() {
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

void RosterListScreen::moveFocus(int direction) {
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

void RosterListScreen::rotaryMoveFocus(int direction) { moveFocus(direction); }

void RosterListScreen::rotaryActivateFocused() {
  if (isCleanedUp || focusedIndex < 0 || focusedIndex >= static_cast<int>(listItems.size())) {
    return;
  }
  selectAddress(listItems[focusedIndex]->getAddress());
}

void RosterListScreen::applyLocoState(int address, int speed, Direction direction, int functionMap) {
  auto *loco = Loco::getByAddress(address);
  if (loco) {
    loco->setSpeed(speed);
    loco->setDirection(direction);
    loco->setFunctionStates(functionMap);
  }

  auto item = getItemByAddress(address);
  if (item) {
    item->updateState(speed, direction, functionMap);
  }
}

void RosterListScreen::requestThrottle(int address, int speed, Direction direction) {
  auto wifiControl = utilities::WifiControl::instance();
  if (!wifiControl->setLocoThrottle(address, speed, direction)) {
    ESP_LOGW(TAG, "Cannot control loco %d while disconnected", address);
    return;
  }

  auto *loco = Loco::getByAddress(address);
  int functionMap = 0;
  if (loco) {
    loco->setSpeed(speed);
    loco->setDirection(direction);
    functionMap = loco->getFunctionStates();
  }

  applyLocoState(address, speed, direction, functionMap);
}

void RosterListScreen::requestStop(int address) {
  auto wifiControl = utilities::WifiControl::instance();
  if (!wifiControl->stopLoco(address)) {
    ESP_LOGW(TAG, "Cannot stop loco %d while disconnected", address);
    return;
  }

  auto *loco = Loco::getByAddress(address);
  Direction direction = Forward;
  int functionMap = 0;
  if (loco) {
    direction = loco->getDirection();
    loco->setSpeed(0);
    functionMap = loco->getFunctionStates();
  }

  applyLocoState(address, 0, direction, functionMap);
}

void RosterListScreen::requestFunction(int address, int function, bool on) {
  auto wifiControl = utilities::WifiControl::instance();
  if (!wifiControl->setLocoFunction(address, function, on)) {
    ESP_LOGW(TAG, "Cannot set function F%d for loco %d while disconnected", function, address);
    return;
  }

  auto *loco = Loco::getByAddress(address);
  int speed = 0;
  Direction direction = Forward;
  int functionMap = 0;
  if (loco) {
    functionMap = loco->getFunctionStates();
    if (on) {
      functionMap |= (1 << function);
    } else {
      functionMap &= ~(1 << function);
    }
    loco->setFunctionStates(functionMap);
    speed = loco->getSpeed();
    direction = loco->getDirection();
  }

  applyLocoState(address, speed, direction, functionMap);
}

} // namespace display