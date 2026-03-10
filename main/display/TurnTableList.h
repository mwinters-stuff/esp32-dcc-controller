#pragma once
#include "Screen.h"
#include <memory>

#include "TurnTableListItem.h"

namespace display {
class TurnTableListScreen : public Screen, public std::enable_shared_from_this<TurnTableListScreen> {

public:
  static std::shared_ptr<TurnTableListScreen> instance() {
    static std::shared_ptr<TurnTableListScreen> s;
    if (!s)
      s.reset(new TurnTableListScreen());
    return s;
  }
  TurnTableListScreen(const TurnTableListScreen &) = delete;
  TurnTableListScreen &operator=(const TurnTableListScreen &) = delete;
  ~TurnTableListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void refreshList();
  void unsubscribeAll();
  std::shared_ptr<TurnTableListItem> getItem(lv_obj_t *bn);

  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

private:
  std::vector<std::shared_ptr<TurnTableListItem>> listItems;
  std::shared_ptr<TurnTableListItem> getItemByTurnTableId(int TurnTableId);

  // void *TurnTable_changed_sub = nullptr;

  bool isCleanedUp = false;
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *list_TurnTables = nullptr;
  lv_obj_t *btn_back = nullptr;
  lv_obj_t *currentButton = nullptr;

protected:
  TurnTableListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurnTableListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurnTableListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }
};
} // namespace display
