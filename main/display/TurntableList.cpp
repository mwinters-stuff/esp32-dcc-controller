#include "TurntableList.h"
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

static const char *TAG = "Turntable_LIST_SCREEN";

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

  refreshList();
}

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
}

void TurntableListScreen::unsubscribeAll() {}

void TurntableListScreen::cleanUp() {
  ESP_LOGI(TAG, "Cleaning up TurntableListScreen");
  isCleanedUp = true;
  unsubscribeAll();
  listItems.clear();
  lbl_title = nullptr;
  list_Turntables = nullptr;
  btn_back = nullptr;
  currentButton = nullptr;
  lv_obj_clean(lvObj_);
}

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

    auto item = getItem(target);
    auto index = std::dynamic_pointer_cast<TurntableIndexListItem>(item);
    if (index) {
      moveToIndex(index);
    } else {
      ESP_LOGW(TAG, "No item found for clicked button");
    }
  }
}

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

std::shared_ptr<IListItem> TurntableListScreen::getItem(lv_obj_t *bn) {
  for (const auto &item : listItems) {
    if (item->getLvObj() == bn) {
      return item;
    }
  }
  return nullptr;
}

std::shared_ptr<TurntableListItem> TurntableListScreen::getItemByTurntableId(int TurntableId) {
  for (const auto &item : listItems) {
    auto turnTableItem = std::dynamic_pointer_cast<TurntableListItem>(item);
    if (turnTableItem && turnTableItem->getId() == TurntableId) {
      return turnTableItem;
    }
  }
  return nullptr;
}

} // namespace display