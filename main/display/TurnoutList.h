#pragma once
#include "RotaryListScreenBase.h"
#include <memory>

#include "TurnoutListItem.h"

namespace display {
class TurnoutListScreen : public RotaryListScreenBase, public std::enable_shared_from_this<TurnoutListScreen> {
public:
  static std::shared_ptr<TurnoutListScreen> instance() {
    static std::shared_ptr<TurnoutListScreen> s;
    if (!s)
      s.reset(new TurnoutListScreen());
    return s;
  }
  TurnoutListScreen(const TurnoutListScreen &) = delete;
  TurnoutListScreen &operator=(const TurnoutListScreen &) = delete;
  ~TurnoutListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void refreshList();
  void unsubscribeAll();
  std::shared_ptr<TurnoutListItem> getItem(lv_obj_t *bn);

  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

private:
  std::vector<std::shared_ptr<TurnoutListItem>> listItems;
  int focusedIndex = -1;

  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override { moveFocus(direction); }
  void rotaryActivateFocused() override { activateFocused(); }
  void throwTurnout(std::shared_ptr<TurnoutListItem> item, bool newThrownState);
  std::shared_ptr<TurnoutListItem> getItemByTurnoutId(int turnoutId);
  void updateFocusedState();
  void moveFocus(int direction);
  void activateFocused();

  lv_msg_sub_dsc_t *turnout_changed_sub = nullptr;

  bool isCleanedUp = false;
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *list_turnouts = nullptr;
  lv_obj_t *btn_back = nullptr;
  lv_obj_t *currentButton = nullptr;

protected:
  TurnoutListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurnoutListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurnoutListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }
};
} // namespace display
