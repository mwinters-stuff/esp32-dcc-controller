#include "RouteList.h"
#include "LvglWrapper.h"
#include "connection/wifi_control.h"
#include "utilities/RotaryEncoder.h"
#include <esp_log.h>

namespace display {

static const char *TAG = "ROUTE_LIST_SCREEN";

static void apply_focus_outline(lv_obj_t *obj, bool focused) {
  if (!obj) {
    return;
  }

  lv_obj_set_style_border_width(obj, focused ? 2 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_opa(obj, focused ? LV_OPA_100 : LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, lv_color_hex(0x35B6FF), LV_PART_MAIN);
  lv_obj_set_style_border_side(obj, focused ? LV_BORDER_SIDE_FULL : LV_BORDER_SIDE_NONE, LV_PART_MAIN);
}

void RouteListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  isCleanedUp = false;
  selectedRouteId = -1;
  listItems.clear();

  lbl_title = makeLabel(lvObj_, "Routes", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  list_routes = makeListView(lvObj_, 0, 40, 320, 380);
  lv_obj_add_event_cb(list_routes, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &RouteListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  refreshList();

  utilities::RotaryEncoder::instance()->setCallbacks(&RouteListScreen::rotary_rotate_trampoline,
                                                     &RouteListScreen::rotary_click_trampoline,
                                                     &RouteListScreen::rotary_long_press_trampoline, this);
}

void RouteListScreen::refreshList() {
  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();

  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot refresh route list");
    return;
  }

  if (!dccProtocol->receivedRouteList()) {
    ESP_LOGW(TAG, "DCC Protocol has not received route list, cannot refresh route list");
    return;
  }

  listItems.clear();
  lv_obj_clean(list_routes);

  for (auto route = DCCExController::Route::getFirst(); route; route = route->getNext()) {
    const char *routeName = route->getName();
    auto listItem = std::make_shared<RouteListItem>(list_routes, listItems.size(), route->getId(),
                                                    routeName ? routeName : "", static_cast<char>(route->getType()));
    listItems.push_back(listItem);
  }

  focusedIndex = listItems.empty() ? -1 : 0;
  updateFocusedState();
  updateSelectedState();
}

void RouteListScreen::unsubscribeAll() {}

void RouteListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up RouteListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_routes = nullptr;
  btn_back = nullptr;
  focusedIndex = -1;
  selectedRouteId = -1;
  pendingRotateSteps.store(0, std::memory_order_relaxed);
  utilities::RotaryEncoder::instance()->clearCallbacks(this);
  lv_obj_clean(lvObj_);
}

void RouteListScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    if (auto screen = parentScreen_.lock()) {
      cleanUp();
      screen->showScreen();
    }
  }
}

void RouteListScreen::button_listitem_click_event_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_obj_t *cont = static_cast<lv_obj_t *>(lv_event_get_current_target(e));

    if (target == cont) {
      return;
    }

    auto item = getItem(target);
    if (item) {
      focusedIndex = static_cast<int>(item->index);
      updateFocusedState();
      startRoute(item);
    }
  }
}

void RouteListScreen::updateFocusedState() {
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

void RouteListScreen::updateSelectedState() {
  for (const auto &item : listItems) {
    auto obj = item->getLvObj();
    if (!obj) {
      continue;
    }

    if (item->getRouteId() == selectedRouteId) {
      lv_obj_add_state(obj, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(obj, LV_STATE_CHECKED);
    }
  }
}

void RouteListScreen::moveFocus(int direction) {
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

void RouteListScreen::startRoute(std::shared_ptr<RouteListItem> item) {
  if (!item) {
    return;
  }

  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();
  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot start route");
    return;
  }

  dccProtocol->startRoute(item->getRouteId());
  selectedRouteId = item->getRouteId();
  updateSelectedState();
}

void RouteListScreen::activateFocused() {
  if (isCleanedUp || focusedIndex < 0 || focusedIndex >= static_cast<int>(listItems.size())) {
    return;
  }

  auto item = listItems[focusedIndex];
  if (item) {
    startRoute(item);
  }
}

void RouteListScreen::goBack() {
  if (isCleanedUp) {
    return;
  }
  if (auto screen = parentScreen_.lock()) {
    cleanUp();
    screen->showScreen();
  }
}

void RouteListScreen::processPendingRotate() {
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

void RouteListScreen::rotary_rotate_trampoline(int32_t delta, void *userData) {
  auto *self = static_cast<RouteListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  self->pendingRotateSteps.fetch_add(delta, std::memory_order_relaxed);
  lv_async_call(&RouteListScreen::rotary_process_trampoline, self);
}

void RouteListScreen::rotary_click_trampoline(void *userData) {
  auto *self = static_cast<RouteListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<RouteListScreen *>(ctx);
        if (screen && !screen->isCleanedUp) {
          screen->activateFocused();
        }
      },
      self);
}

void RouteListScreen::rotary_long_press_trampoline(void *userData) {
  auto *self = static_cast<RouteListScreen *>(userData);
  if (!self || self->isCleanedUp) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<RouteListScreen *>(ctx);
        if (screen && !screen->isCleanedUp) {
          screen->goBack();
        }
      },
      self);
}

void RouteListScreen::rotary_process_trampoline(void *userData) {
  auto *self = static_cast<RouteListScreen *>(userData);
  if (self && !self->isCleanedUp) {
    self->processPendingRotate();
  }
}

std::shared_ptr<RouteListItem> RouteListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

} // namespace display
