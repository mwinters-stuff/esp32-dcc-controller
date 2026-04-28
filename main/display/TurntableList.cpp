/**
 * @file TurntableList.cpp
 * @brief Screen displaying turntables and their indexed positions.
 *
 * Each turntable entry expands to show its named index positions. Tapping or
 * confirming with the rotary encoder sends a move-to-index command. Supports
 * rotary-encoder focus navigation.
 */
#include "TurntableList.h"
#include "DCCMenu.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "Screen.h"
#include "WaitingScreen.h"
#include "connection/wifi_control.h"
#include "definitions.h"
#include "utilities/RotaryEncoder.h"
#include "utilities/WifiHandler.h"
#include <cstdio>
#include <memory>
#include <vector>

namespace display {

static const char *TAG = "Turntable_LIST_SCREEN";

// Draws or clears the focus outline on a turntable list item.
static void apply_focus_outline(lv_obj_t *obj, bool focused) {
  if (!obj) {
    return;
  }

  lv_obj_set_style_border_width(obj, focused ? 2 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_opa(obj, focused ? LV_OPA_100 : LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, lv_color_hex(0x35B6FF), LV_PART_MAIN);
  lv_obj_set_style_border_side(obj, focused ? LV_BORDER_SIDE_FULL : LV_BORDER_SIDE_NONE, LV_PART_MAIN);
}

// Builds the turntable list UI, registers rotary callbacks and subscribes to
// turntable updated / move messages.
void TurntableListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;

  listItems.clear();
  currentButton = nullptr;

  // Title
  lbl_title = makeLabel(lvObj_, "Turntables", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  // List for Auto Detect
  list_Turntables = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_Turntables, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  // Bottom Buttons
  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &TurntableListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  turntable_changed_sub = lv_msg_subscribe(
      MSG_DCC_TURNTABLE_CHANGED,
      [](lv_msg_t *msg) {
        TurntableListScreen *self = static_cast<TurntableListScreen *>(lv_msg_get_user_data(msg));
        if (!self || self->isCleanedUp)
          return;
        ESP_LOGI(TAG, "Turntable changed message received");
        TurntableActionData *data = static_cast<TurntableActionData *>(const_cast<void *>(lv_msg_get_payload(msg)));
        ESP_LOGI(TAG, "Turntable ID=%d position=%d moving=%s", data->turntableId, data->position,
                 data->moving ? "moving" : "stopped");
        auto item = self->getIndexItemByTurntableAndIndex(data->turntableId, data->position);
        if (item) {
          if (data->moving) {
            self->startTurntableFlashing(data->turntableId, data->position);
          } else {
            self->startTurntableFlashing(data->turntableId, data->position);
            self->stopTurntableFlashing(true);
          }
        } else {
          ESP_LOGW(TAG, "No item found for turntable ID %d Position %d", data->turntableId, data->position);
        }
      },
      this);

  refreshList();
  utilities::RotaryEncoder::instance()->setCallbacks(&TurntableListScreen::rotary_rotate_trampoline,
                                                     &TurntableListScreen::rotary_click_trampoline,
                                                     &TurntableListScreen::rotary_long_press_trampoline, this);
}

// Clears and repopulates the list widget from the latest turntable data.
void TurntableListScreen::refreshList() {
  ESP_LOGI(TAG, "Refreshing Turntable list");
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh Turntable list");
    return;
  }

  if (dccProtocol->receivedTurntableList() == false) {
    ESP_LOGW(TAG, "DCC Protocol has not received Turntable list, cannot refresh Turntable list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_Turntables);

  for (auto Turntable = DCCExController::Turntable::getFirst(); Turntable; Turntable = Turntable->getNext()) {
    ESP_LOGI(TAG, "Turntable ID=%d, Name=%s", Turntable->getId(), Turntable->getName());

    auto listItem = std::make_shared<TurntableListItem>(list_Turntables, listItems.size(), Turntable->getId(),
                                                        Turntable->getName());
    listItems.push_back(listItem);
    for (auto index = Turntable->getFirstIndex(); index; index = index->getNextIndex()) {
      ESP_LOGI(TAG, "  Index ID=%d Name=%s", index->getId(), index->getName());

      auto indexListItem = std::make_shared<TurntableIndexListItem>(
          list_Turntables, listItems.size(), Turntable->getId(), index->getId(), index->getName());
      listItems.push_back(indexListItem);
    }
  }

  focusedIndex = -1;
  for (size_t i = 0; i < listItems.size(); ++i) {
    if (std::dynamic_pointer_cast<TurntableIndexListItem>(listItems[i])) {
      focusedIndex = static_cast<int>(i);
      break;
    }
  }
  updateFocusedState();
}

// Removes turntable-related lv_msg subscriptions.
void TurntableListScreen::unsubscribeAll() {
  if (turntable_changed_sub) {
    lv_msg_unsubscribe(turntable_changed_sub);
    turntable_changed_sub = nullptr;
  }
}

// Releases widget pointers, unregisters rotary callbacks and clears item lists.
void TurntableListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up TurntableListScreen");
  isCleanedUp = true;
  stopTurntableFlashing(false);
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_Turntables = nullptr;
  btn_back = nullptr;
  currentButton = nullptr;
  focusedIndex = -1;
  pendingRotateSteps.store(0, std::memory_order_relaxed);
  utilities::RotaryEncoder::instance()->clearCallbacks(this);
  lv_obj_clean(lvObj_);
}

// Returns to the previous screen.
void TurntableListScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

// Handles touch taps on list items: activates the tapped entry.
void TurntableListScreen::button_listitem_click_event_callback(lv_event_t *e) {
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
    ESP_LOGI(TAG, "Moving To: %s", lv_list_get_btn_text(list_Turntables, target));

    activateItem(target);
  }
}

// Sends a move-to-index command for the given item to the DCC server.
void TurntableListScreen::activateItem(lv_obj_t *target) {
  auto item = getItem(target);
  auto index = std::dynamic_pointer_cast<TurntableIndexListItem>(item);
  if (index) {
    focusedIndex = static_cast<int>(item->index);
    updateFocusedState();

    if (isTurntableMoving(index->getTurntableId())) {
      ESP_LOGW(TAG, "Ignoring index click while turntable ID %d is moving", index->getTurntableId());
      return;
    }

    const bool isSelectedIndex = lv_obj_has_state(index->getLvObj(), LV_STATE_CHECKED) ||
                                 (index->getTurntableId() == flashingTurntableId && index->getId() == flashingIndexId);
    if (isSelectedIndex) {
      auto wifiControl = utilities::WifiControl::instance();
      auto dccProtocol = wifiControl->dccProtocol();
      if (dccProtocol) {
        char command[24];
        snprintf(command, sizeof(command), "I %d 0 18", index->getTurntableId());
        ESP_LOGI(TAG, "Re-click on highlighted turntable index, sending reverse command: <%s>", command);
        dccProtocol->sendCommand(command);
      } else {
        ESP_LOGW(TAG, "DCC Protocol is null, cannot send turntable reverse command");
      }
      return;
    }

    moveToIndex(index);
  } else {
    ESP_LOGW(TAG, "No item found for clicked button");
  }
}

// Refreshes focus outlines across all list items.
void TurntableListScreen::updateFocusedState() {
  for (size_t i = 0; i < listItems.size(); ++i) {
    auto obj = listItems[i]->getLvObj();
    if (!obj) {
      continue;
    }
    if (static_cast<int>(i) == focusedIndex) {
      lv_obj_add_state(obj, LV_STATE_FOCUSED);
      apply_focus_outline(obj, true);
      lv_obj_scroll_to_view(obj, LV_ANIM_OFF);
    } else {
      lv_obj_clear_state(obj, LV_STATE_FOCUSED);
      apply_focus_outline(obj, false);
    }
  }
}

// Moves the rotary focus by `direction` steps (+1 down, -1 up).
void TurntableListScreen::moveFocus(int direction) {
  if (isCleanedUp || listItems.empty() || direction == 0) {
    return;
  }

  int index = focusedIndex;
  if (index < 0 || index >= static_cast<int>(listItems.size()) ||
      !std::dynamic_pointer_cast<TurntableIndexListItem>(listItems[index])) {
    index = -1;
  }

  for (size_t attempts = 0; attempts < listItems.size(); ++attempts) {
    index += direction;
    if (index >= static_cast<int>(listItems.size())) {
      index = 0;
    } else if (index < 0) {
      index = static_cast<int>(listItems.size()) - 1;
    }

    if (std::dynamic_pointer_cast<TurntableIndexListItem>(listItems[index])) {
      focusedIndex = index;
      updateFocusedState();
      return;
    }
  }
}

// Triggers the action for the currently focused item.
void TurntableListScreen::activateFocused() {
  if (isCleanedUp || focusedIndex < 0 || focusedIndex >= static_cast<int>(listItems.size())) {
    return;
  }

  auto item = listItems[focusedIndex];
  if (item) {
    activateItem(item->getLvObj());
  }
}

// Returns to the previous screen via the standard back path.
void TurntableListScreen::goBack() {
  if (isCleanedUp) {
    return;
  }
  if (auto screen = parentScreen_.lock()) {
    cleanUp();
    screen->showScreen();
  }
}

// Drains the pending rotation count and moves focus accordingly.
void TurntableListScreen::processPendingRotate() {
  int32_t steps = pendingRotateSteps.exchange(0, std::memory_order_relaxed);
  while (steps > 0) {
    moveFocus(1);
    --steps;
  }
  while (steps < 0) {
    moveFocus(-1);
    ++steps;
  }
}

// Rotary encoder rotate ISR trampoline: accumulates delta into pendingRotate.
void TurntableListScreen::rotary_rotate_trampoline(int32_t delta, void *userData) {
  auto *self = static_cast<TurntableListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  self->pendingRotateSteps.fetch_add(delta, std::memory_order_relaxed);
  lv_async_call(&TurntableListScreen::rotary_process_trampoline, self);
}

// Rotary encoder click trampoline: activates the focused turntable index.
void TurntableListScreen::rotary_click_trampoline(void *userData) {
  auto *self = static_cast<TurntableListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<TurntableListScreen *>(ctx);
        if (screen && !screen->isCleanedUp) {
          screen->activateFocused();
        }
      },
      self);
}

// Rotary encoder long-press trampoline: navigates back.
void TurntableListScreen::rotary_long_press_trampoline(void *userData) {
  auto *self = static_cast<TurntableListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<TurntableListScreen *>(ctx);
        if (screen && !screen->isCleanedUp) {
          screen->goBack();
        }
      },
      self);
}

// LVGL async trampoline that flushes pending rotate events on the LVGL thread.
void TurntableListScreen::rotary_process_trampoline(void *userData) {
  auto *self = static_cast<TurntableListScreen *>(userData);
  if (self && !self->isCleanedUp) {
    self->processPendingRotate();
  }
}

// Sends a move-to-index command for the given index item to the DCC server.
void TurntableListScreen::moveToIndex(std::shared_ptr<TurntableIndexListItem> index) {
  ESP_LOGI(TAG, "Moving to Turntable ID %d Index ID %d", index->getTurntableId(), index->getId());
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();
  if (dccProtocol) {
    auto turnTable = DCCExController::Turntable::getById(index->getTurntableId());
    if (turnTable) {
      auto turnTableIndex = turnTable->getIndexById(index->getId());
      if (turnTableIndex) {
        dccProtocol->rotateTurntable(turnTable->getId(), turnTableIndex->getId());
      } else {
        ESP_LOGW(TAG, "No Turntable index found for ID %d", index->getId());
      }
    } else {
      ESP_LOGW(TAG, "No Turntable found for ID %d", index->getTurntableId());
    }
  } else {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot move to Turntable index");
  }
}

// Returns the list item (turntable or index) whose LVGL button matches bn.
std::shared_ptr<IListItem> TurntableListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

// Returns the TurntableListItem matching the given DCC turntable ID, or nullptr.
std::shared_ptr<TurntableListItem> TurntableListScreen::getItemByTurntableId(int TurntableId) {
  for (const auto &item : listItems) {
    auto turnTableItem = std::dynamic_pointer_cast<TurntableListItem>(item);
    if (turnTableItem && turnTableItem->getId() == TurntableId) {
      return turnTableItem;
    }
  }
  return nullptr;
}

// Returns the index item matching the given turntable ID and index number.
std::shared_ptr<TurntableIndexListItem> TurntableListScreen::getIndexItemByTurntableAndIndex(int turntableId,
                                                                                             int indexId) {
  for (const auto &item : listItems) {
    auto indexItem = std::dynamic_pointer_cast<TurntableIndexListItem>(item);
    if (indexItem && indexItem->getTurntableId() == turntableId && indexItem->getId() == indexId) {
      return indexItem;
    }
  }
  return nullptr;
}

void TurntableListScreen::setTurntableIndexCheckedState(int turntableId, int indexId, bool checked) {
  for (const auto &item : listItems) {
    auto indexItem = std::dynamic_pointer_cast<TurntableIndexListItem>(item);
    if (!indexItem) {
      continue;
    }

    if (indexItem->getTurntableId() == turntableId && indexItem->getId() == indexId) {
      if (checked) {
        lv_obj_add_state(indexItem->getLvObj(), LV_STATE_CHECKED);
      } else {
        lv_obj_clear_state(indexItem->getLvObj(), LV_STATE_CHECKED);
      }
      return;
    }
  }
}

void TurntableListScreen::setExclusiveTurntableIndexChecked(int turntableId, int indexId) {
  for (const auto &item : listItems) {
    auto indexItem = std::dynamic_pointer_cast<TurntableIndexListItem>(item);
    if (!indexItem) {
      continue;
    }

    if (indexItem->getTurntableId() != turntableId) {
      continue;
    }

    if (indexItem->getId() == indexId) {
      lv_obj_add_state(indexItem->getLvObj(), LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(indexItem->getLvObj(), LV_STATE_CHECKED);
    }
  }
}

void TurntableListScreen::startTurntableFlashing(int turntableId, int indexId) {
  if (flashingTurntableId != turntableId || flashingIndexId != indexId) {
    stopTurntableFlashing(false);
    flashingTurntableId = turntableId;
    flashingIndexId = indexId;
  }

  setExclusiveTurntableIndexChecked(turntableId, indexId);
  flashCheckedState = true;

  if (!turntable_flash_timer) {
    turntable_flash_timer = lv_timer_create(&TurntableListScreen::flash_timer_trampoline, 300, this);
  } else {
    lv_timer_resume(turntable_flash_timer);
  }
}

void TurntableListScreen::stopTurntableFlashing(bool keepHighlighted) {
  if (turntable_flash_timer) {
    lv_timer_del(turntable_flash_timer);
    turntable_flash_timer = nullptr;
  }

  if (flashingTurntableId >= 0) {
    if (keepHighlighted) {
      setExclusiveTurntableIndexChecked(flashingTurntableId, flashingIndexId);
    } else {
      setTurntableIndexCheckedState(flashingTurntableId, flashingIndexId, false);
    }
  }

  flashingTurntableId = -1;
  flashingIndexId = -1;
  flashCheckedState = false;
}

void TurntableListScreen::onFlashTimerTick() {
  if (isCleanedUp || flashingTurntableId < 0) {
    return;
  }

  auto item = getIndexItemByTurntableAndIndex(flashingTurntableId, flashingIndexId);
  if (!item) {
    return;
  }

  flashCheckedState = !flashCheckedState;
  if (flashCheckedState) {
    lv_obj_add_state(item->getLvObj(), LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(item->getLvObj(), LV_STATE_CHECKED);
  }
}

bool TurntableListScreen::isTurntableMoving(int turntableId) const {
  return turntable_flash_timer != nullptr && flashingTurntableId == turntableId;
}

} // namespace display