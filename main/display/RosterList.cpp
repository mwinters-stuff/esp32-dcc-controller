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
#include <esp_timer.h>
#include <memory>
#include <vector>

namespace {
constexpr int64_t kThrottleMinStepIntervalUs = 20 * 1000;
constexpr UBaseType_t kThrottleEventQueueLen = 32;

enum ThrottleEventType : int {
  ThrottleEventRotate = 1,
  ThrottleEventClick = 2,
  ThrottleEventDoubleClick = 3,
  ThrottleEventLongPress = 4,
};

struct ThrottleEvent {
  int type;
  int32_t delta;
};

int throttle_step_for_speed(int speed) {
  if (speed <= 10) {
    return 1;
  }
  if (speed <= 20) {
    return 2;
  }
  if (speed <= 50) {
    return 3;
  }
  return 5;
}
} // namespace

namespace display {

static const char *TAG = "ROSTER_LIST_SCREEN";

// Builds the expandable roster list UI and subscribes to roster/loco updates.
void RosterListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  isCleanedUp = false;

  listItems.clear();
  focusedIndex = -1;
  expandedAddress = -1;
  locoPollCursor_ = 0;

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
  attachThrottleEncoder();
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
      // One-shot sync: request current server state for each roster item.
      wifiControl->requestLocoUpdate(loco->getAddress());
    } else {
      ESP_LOGI(TAG, "Skipping non-roster loco with ID=%d", loco->getAddress());
    }
    loco = loco->getNext();
  }

  if (listItems.empty()) {
    focusedIndex = -1;
    expandedAddress = -1;
    locoPollCursor_ = 0;
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
  detachThrottleEncoder();
  rotaryDetach();
  unsubscribeAll();
  listItems.clear();
  focusedIndex = -1;
  expandedAddress = -1;
  locoPollCursor_ = 0;
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
  const int previousExpandedAddress = expandedAddress;
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

  ESP_LOGI(TAG, "selectAddress addr=%d prevExpanded=%d newExpanded=%d focusedIndex=%d itemCount=%u", address,
           previousExpandedAddress, expandedAddress, focusedIndex, static_cast<unsigned>(listItems.size()));

  updateExpandedState();
  updateFocusedState();
  updateActiveThrottleAddress();
}

void RosterListScreen::updateExpandedState() {
  for (const auto &item : listItems) {
    const bool expanded = expandedAddress >= 0 && item->getAddress() == expandedAddress;
    ESP_LOGI(TAG, "updateExpandedState item=%d expanded=%d targetExpanded=%d", item->getAddress(), expanded ? 1 : 0,
             expandedAddress);
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
  updateActiveThrottleAddress();
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
  auto *loco = Loco::getByAddress(address);
  const int currentSpeed = loco ? loco->getSpeed() : speed;
  const Direction currentDirection = loco ? loco->getDirection() : direction;
  if (currentDirection != direction && currentSpeed > 0) {
    ESP_LOGI(TAG, "Ignoring direction change for loco %d while speed is %d", address, currentSpeed);
    return;
  }

  auto wifiControl = utilities::WifiControl::instance();
  if (!wifiControl->setLocoThrottle(address, speed, direction)) {
    ESP_LOGW(TAG, "Cannot control loco %d while disconnected", address);
    return;
  }
}

void RosterListScreen::requestStop(int address) {
  auto wifiControl = utilities::WifiControl::instance();
  if (!wifiControl->stopLoco(address)) {
    ESP_LOGW(TAG, "Cannot stop loco %d while disconnected", address);
    return;
  }
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

int RosterListScreen::activeAddressForThrottle() const {
  int address = -1;
  if (expandedAddress >= 0) {
    address = expandedAddress;
  } else if (focusedIndex >= 0 && focusedIndex < static_cast<int>(listItems.size())) {
    address = listItems[focusedIndex]->getAddress();
  } else if (!listItems.empty()) {
    address = listItems.front()->getAddress();
  }

  ESP_LOGI(TAG, "activeAddressForThrottle expanded=%d focusedIndex=%d result=%d itemCount=%u", expandedAddress,
           focusedIndex, address, static_cast<unsigned>(listItems.size()));
  return address;
}

void RosterListScreen::updateActiveThrottleAddress() {
  activeThrottleAddress_.store(activeAddressForThrottle(), std::memory_order_relaxed);
}

void RosterListScreen::attachThrottleEncoder() {
#if CONFIG_ROTARY_ENCODER_2_ENABLE
  pendingThrottleSteps_.store(0, std::memory_order_relaxed);
  throttleProcessQueued_.store(false, std::memory_order_relaxed);
  updateActiveThrottleAddress();
  lastThrottleStepUs_ = esp_timer_get_time();

  const bool initOk = throttleEncoder_.init(
      static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_2_GPIO_A), static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_2_GPIO_B),
      CONFIG_ROTARY_ENCODER_2_DEFAULT_DIRECTION == 1,
#if CONFIG_ROTARY_ENCODER_2_SW_ENABLE
      true, static_cast<gpio_num_t>(CONFIG_ROTARY_ENCODER_2_GPIO_SW), CONFIG_ROTARY_ENCODER_2_SW_ACTIVE_LEVEL
#else
      false, GPIO_NUM_NC, 0
#endif
  );

  if (!initOk) {
    ESP_LOGW(TAG, "Failed to initialize second rotary encoder");
    throttleEncoderAttached_ = false;
    return;
  }

  throttleEncoder_.setCallbacks(
      &RosterListScreen::throttle_rotate_trampoline, &RosterListScreen::throttle_click_trampoline,
      &RosterListScreen::throttle_long_press_trampoline, this, &RosterListScreen::throttle_double_click_trampoline);

  throttleEncoder_.setActivityCallback(
      [](void *) {
        app_note_user_activity();
      },
      nullptr);

  throttleEventQueue_ = xQueueCreate(kThrottleEventQueueLen, sizeof(ThrottleEvent));
  if (throttleEventQueue_ == nullptr) {
    ESP_LOGW(TAG, "Failed to create encoder-2 event queue");
    throttleEncoder_.clearCallbacks(this);
    throttleEncoder_.setActivityCallback(nullptr, nullptr);
    throttleEncoder_.deinit();
    throttleEncoderAttached_ = false;
    return;
  }

  throttleEventTaskRunning_.store(true, std::memory_order_relaxed);
  if (xTaskCreate(&RosterListScreen::throttle_event_task_trampoline, "roster_thr2_evt", 4096, this,
                  tskIDLE_PRIORITY + 1, &throttleEventTask_) != pdPASS) {
    ESP_LOGW(TAG, "Failed to create encoder-2 event task");
    throttleEventTaskRunning_.store(false, std::memory_order_relaxed);
    vQueueDelete(throttleEventQueue_);
    throttleEventQueue_ = nullptr;
    throttleEncoder_.clearCallbacks(this);
    throttleEncoder_.setActivityCallback(nullptr, nullptr);
    throttleEncoder_.deinit();
    throttleEncoderAttached_ = false;
    return;
  }

  throttleEncoderAttached_ = true;
  ESP_LOGI(TAG, "Second rotary encoder attached for roster throttle (dedicated queue task)");
#endif
}

void RosterListScreen::detachThrottleEncoder() {
#if CONFIG_ROTARY_ENCODER_2_ENABLE
  if (!throttleEncoderAttached_) {
    return;
  }

  pendingThrottleSteps_.store(0, std::memory_order_relaxed);
  throttleProcessQueued_.store(false, std::memory_order_relaxed);
  throttleEventTaskRunning_.store(false, std::memory_order_relaxed);
  if (throttleEventTask_ != nullptr) {
    vTaskDelete(throttleEventTask_);
    throttleEventTask_ = nullptr;
  }
  if (throttleEventQueue_ != nullptr) {
    vQueueDelete(throttleEventQueue_);
    throttleEventQueue_ = nullptr;
  }
  throttleEncoder_.clearCallbacks(this);
  throttleEncoder_.setActivityCallback(nullptr, nullptr);
  throttleEncoder_.deinit();
  throttleEncoderAttached_ = false;
#endif
}

bool RosterListScreen::enqueueThrottleEvent(int type, int32_t delta) {
  if (throttleEventQueue_ == nullptr || !throttleEventTaskRunning_.load(std::memory_order_relaxed)) {
    return false;
  }
  ThrottleEvent ev{type, delta};
  return xQueueSend(throttleEventQueue_, &ev, 0) == pdTRUE;
}

void RosterListScreen::throttle_event_task_trampoline(void *arg) {
  auto *self = static_cast<RosterListScreen *>(arg);
  if (self) {
    self->throttleEventTask();
  }
  vTaskDelete(nullptr);
}

void RosterListScreen::throttleEventTask() {
  while (throttleEventTaskRunning_.load(std::memory_order_relaxed)) {
    ThrottleEvent ev{};
    if (xQueueReceive(throttleEventQueue_, &ev, pdMS_TO_TICKS(50)) != pdTRUE) {
      continue;
    }

    if (isCleanedUp) {
      continue;
    }

    const int address = activeThrottleAddress_.load(std::memory_order_relaxed);
    if (address < 0) {
      continue;
    }

    if (ev.type == ThrottleEventRotate) {
      auto *loco = Loco::getByAddress(address);
      int speed = loco ? loco->getSpeed() : 0;
      Direction direction = loco ? loco->getDirection() : Forward;

      int newSpeed = speed;
      const int directionStep = ev.delta > 0 ? 1 : -1;
      int remaining = ev.delta > 0 ? ev.delta : -ev.delta;
      while (remaining-- > 0) {
        const int stepAmount = throttle_step_for_speed(newSpeed);
        newSpeed += directionStep * stepAmount;
        if (newSpeed < 0) {
          newSpeed = 0;
          break;
        }
        if (newSpeed > 126) {
          newSpeed = 126;
          break;
        }
      }

      requestThrottle(address, newSpeed, direction);
      continue;
    }

    if (ev.type == ThrottleEventClick || ev.type == ThrottleEventDoubleClick) {
      auto *loco = Loco::getByAddress(address);
      const int speed = loco ? loco->getSpeed() : 0;
      const Direction direction = loco ? loco->getDirection() : Forward;
      const Direction toggledDirection = direction == Forward ? Reverse : Forward;
      requestThrottle(address, speed, toggledDirection);
      continue;
    }

    if (ev.type == ThrottleEventLongPress) {
      requestStop(address);
      continue;
    }
  }
}

void RosterListScreen::processPendingThrottleSteps() {
  throttleProcessQueued_.store(false, std::memory_order_relaxed);

  if (isCleanedUp) {
    pendingThrottleSteps_.store(0, std::memory_order_relaxed);
    return;
  }

  const int address = activeAddressForThrottle();
  if (address < 0) {
    pendingThrottleSteps_.store(0, std::memory_order_relaxed);
    return;
  }

  int32_t steps = pendingThrottleSteps_.exchange(0, std::memory_order_relaxed);
  if (steps == 0) {
    return;
  }

  ESP_LOGI(TAG, "processPendingThrottleSteps rawSteps=%ld", static_cast<long>(steps));

  constexpr int32_t kMaxStepsPerDispatch = 12;
  if (steps > kMaxStepsPerDispatch) {
    pendingThrottleSteps_.fetch_add(steps - kMaxStepsPerDispatch, std::memory_order_relaxed);
    steps = kMaxStepsPerDispatch;
  } else if (steps < -kMaxStepsPerDispatch) {
    pendingThrottleSteps_.fetch_add(steps + kMaxStepsPerDispatch, std::memory_order_relaxed);
    steps = -kMaxStepsPerDispatch;
  }

  auto *loco = Loco::getByAddress(address);
  int speed = loco ? loco->getSpeed() : 0;
  Direction direction = loco ? loco->getDirection() : Forward;

  int newSpeed = speed;
  const int directionStep = steps > 0 ? 1 : -1;
  int remaining = steps > 0 ? steps : -steps;
  while (remaining-- > 0) {
    const int stepAmount = throttle_step_for_speed(newSpeed);
    newSpeed += directionStep * stepAmount;
    if (newSpeed < 0) {
      newSpeed = 0;
      break;
    }
    if (newSpeed > 126) {
      newSpeed = 126;
      break;
    }
  }

  ESP_LOGI(TAG, "processPendingThrottleSteps address=%d speed=%d newSpeed=%d direction=%d", address, speed, newSpeed,
           static_cast<int>(direction));

  requestThrottle(address, newSpeed, direction);

  if (pendingThrottleSteps_.load(std::memory_order_relaxed) != 0 && !throttleProcessQueued_.exchange(true)) {
    if (lv_async_call(&RosterListScreen::throttle_process_trampoline, this) != LV_RESULT_OK) {
      throttleProcessQueued_.store(false, std::memory_order_relaxed);
      ESP_LOGW(TAG, "throttle process requeue failed");
    }
  }
}

void RosterListScreen::handleThrottleEncoderClick() {
  if (isCleanedUp) {
    return;
  }

  const int address = activeAddressForThrottle();
  if (address < 0) {
    return;
  }

  auto *loco = Loco::getByAddress(address);
  const int speed = loco ? loco->getSpeed() : 0;
  const Direction direction = loco ? loco->getDirection() : Forward;
  const Direction toggledDirection = direction == Forward ? Reverse : Forward;
  ESP_LOGI(TAG, "handleThrottleEncoderClick address=%d speed=%d direction=%d toggled=%d", address, speed,
           static_cast<int>(direction), static_cast<int>(toggledDirection));
  requestThrottle(address, speed, toggledDirection);
}

void RosterListScreen::handleThrottleEncoderLongPress() {
  if (isCleanedUp) {
    return;
  }

  const int address = activeAddressForThrottle();
  if (address < 0) {
    return;
  }

  ESP_LOGI(TAG, "handleThrottleEncoderLongPress address=%d", address);

  requestStop(address);
}

void RosterListScreen::throttle_rotate_trampoline(int32_t delta, void *userData) {
  auto *self = static_cast<RosterListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  const int64_t nowUs = esp_timer_get_time();
  if ((nowUs - self->lastThrottleStepUs_) < kThrottleMinStepIntervalUs) {
    return;
  }
  self->lastThrottleStepUs_ = nowUs;

  ESP_LOGI(TAG, "throttle_rotate_trampoline delta=%ld", static_cast<long>(delta));
  if (!self->enqueueThrottleEvent(ThrottleEventRotate, delta)) {
    ESP_LOGW(TAG, "encoder-2 rotate event dropped");
  }
}

void RosterListScreen::throttle_click_trampoline(void *userData) {
  auto *self = static_cast<RosterListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  if (!self->enqueueThrottleEvent(ThrottleEventClick, 0)) {
    ESP_LOGW(TAG, "encoder-2 click event dropped");
  }
}

void RosterListScreen::throttle_double_click_trampoline(void *userData) {
  auto *self = static_cast<RosterListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  if (!self->enqueueThrottleEvent(ThrottleEventDoubleClick, 0)) {
    ESP_LOGW(TAG, "encoder-2 double-click event dropped");
  }
}

void RosterListScreen::throttle_long_press_trampoline(void *userData) {
  auto *self = static_cast<RosterListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  if (!self->enqueueThrottleEvent(ThrottleEventLongPress, 0)) {
    ESP_LOGW(TAG, "encoder-2 long-press event dropped");
  }
}

void RosterListScreen::throttle_process_trampoline(void *userData) {
  auto *self = static_cast<RosterListScreen *>(userData);
  if (self) {
    self->processPendingThrottleSteps();
  }
}

} // namespace display