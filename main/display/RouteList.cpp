/**
 * @file RouteList.cpp
 * @brief Screen displaying the automation/route list from the DCC-EX server.
 *
 * Allows the user to start, pause and resume routes. Supports rotary-encoder
 * focus navigation and a dedicated pause/resume button that reflects the state
 * of the currently selected route.
 */
#include "RouteList.h"
#include "LvglWrapper.h"
#include "connection/wifi_control.h"
#include <cstdio>
#include <esp_log.h>

namespace display {

static const char *TAG = "ROUTE_LIST_SCREEN";

// Builds the route list UI, sets up rotary callbacks and subscribes to route
// updated / status messages.
void RouteListScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  isCleanedUp = false;
  routesPaused = false;
  selectedRouteId = -1;
  listItems.clear();

  lbl_title = makeLabel(lvObj_, "Routes", LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);

  list_routes = makeListView(lvObj_, 0, 40, 320, 340);
  lv_obj_add_event_cb(list_routes, event_listitem_click_trampoline, LV_EVENT_CLICKED, this);

  lbl_status = makeLabel(lvObj_, "Select a route", LV_ALIGN_BOTTOM_MID, 0, -58, "label.main");

  btn_back = makeButton(lvObj_, "Back", 100, 40, LV_ALIGN_BOTTOM_LEFT, 8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_back, &RouteListScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  btn_pause_resume = makeButton(lvObj_, "Pause", 120, 40, LV_ALIGN_BOTTOM_RIGHT, -8, -12, "button.secondary");
  lv_obj_add_event_cb(btn_pause_resume, &RouteListScreen::event_pause_resume_trampoline, LV_EVENT_CLICKED, this);

  refreshList();
  updatePauseResumeButton();

  rotaryAttach();
}

// Clears and repopulates the list widget from the latest route data.
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

// No message subscriptions to remove for RouteList.
void RouteListScreen::unsubscribeAll() {}

// Releases widget pointers, unregisters rotary callbacks and clears item list.
void RouteListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up RouteListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  lbl_status = nullptr;
  list_routes = nullptr;
  btn_back = nullptr;
  btn_pause_resume = nullptr;
  focusedIndex = -1;
  selectedRouteId = -1;
  routesPaused = false;
  rotaryDetach();
  lv_obj_clean(lvObj_);
}

// Returns to the previous screen.
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

// Toggles the pause/resume state of the focused route.
void RouteListScreen::button_pause_resume_callback(lv_event_t *e) {
  if (isCleanedUp)
    return;
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    auto wifiControl = utilities::WifiControl::instance();
    auto dccProtocol = wifiControl->dccProtocol();
    if (dccProtocol == nullptr) {
      ESP_LOGW(TAG, "DCC Protocol is null, cannot toggle route pause state");
      setStatusText("DCC not connected");
      return;
    }

    if (routesPaused) {
      dccProtocol->resumeRoutes();
      routesPaused = false;
      setStatusText("Routes resumed");
    } else {
      dccProtocol->pauseRoutes();
      routesPaused = true;
      setStatusText("Routes paused");
    }
    updatePauseResumeButton();
  }
}

// Handles touch taps on list items: selects the tapped entry.
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

// Refreshes the visual focus indicator across all items.
void RouteListScreen::updateFocusedState() {
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

// Refreshes the visual selection state across all items.
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

// Moves the rotary focus by `direction` steps (+1 down, -1 up).
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

// Sends the start-route command for the given item to the DCC server.
void RouteListScreen::startRoute(std::shared_ptr<RouteListItem> item) {
  if (!item) {
    return;
  }

  auto wifiControl = utilities::WifiControl::instance();
  auto dccProtocol = wifiControl->dccProtocol();
  if (dccProtocol == nullptr) {
    ESP_LOGW(TAG, "DCC Protocol is null, cannot start route");
    setStatusText("DCC not connected");
    return;
  }

  dccProtocol->startRoute(item->getRouteId());
  routesPaused = false;
  updatePauseResumeButton();
  selectedRouteId = item->getRouteId();
  updateSelectedState();

  char status[64];
  std::snprintf(status, sizeof(status), "Started: %s", item->getDisplayName().c_str());
  setStatusText(status);
}

// Triggers the action for the currently focused list item.
void RouteListScreen::activateFocused() {
  if (isCleanedUp || focusedIndex < 0 || focusedIndex >= static_cast<int>(listItems.size())) {
    return;
  }

  auto item = listItems[focusedIndex];
  if (item) {
    startRoute(item);
  }
}

// Syncs the pause/resume button label and enabled state with the selected route.
void RouteListScreen::updatePauseResumeButton() {
  if (!btn_pause_resume) {
    return;
  }
  lv_label_set_text(lv_obj_get_child(btn_pause_resume, 0), routesPaused ? "Resume" : "Pause");
}

// Updates the status text label shown beneath the list.
void RouteListScreen::setStatusText(const char *text) {
  if (lbl_status) {
    lv_label_set_text(lbl_status, text ? text : "");
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
