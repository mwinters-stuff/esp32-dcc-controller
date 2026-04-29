#pragma once
#include "RotaryListScreenBase.h"
#include <memory>

#include "ListItemBase.h"
#include "TurntableListItem.h"

namespace display {
class TurntableListScreen : public RotaryListScreenBase, public std::enable_shared_from_this<TurntableListScreen> {

public:
  static std::shared_ptr<TurntableListScreen> instance() {
    static std::shared_ptr<TurntableListScreen> s;
    if (!s)
      s.reset(new TurntableListScreen());
    return s;
  }
  TurntableListScreen(const TurntableListScreen &) = delete;
  TurntableListScreen &operator=(const TurntableListScreen &) = delete;
  ~TurntableListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void refreshList();
  void unsubscribeAll();
  std::shared_ptr<IListItem> getItem(lv_obj_t *bn);

  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

  void moveToIndex(std::shared_ptr<TurntableIndexListItem> index);

private:
  void setTurntableIndexCheckedState(int turntableId, int indexId, bool checked);
  void setExclusiveTurntableIndexChecked(int turntableId, int indexId);
  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override { moveFocus(direction); }
  void rotaryActivateFocused() override { activateFocused(); }
  void updateFocusedState();
  void moveFocus(int direction);
  void activateFocused();
  void activateItem(lv_obj_t *target);
  void startTurntableFlashing(int turntableId, int indexId);
  void stopTurntableFlashing(bool keepHighlighted);
  void onFlashTimerTick();
  bool isTurntableMoving(int turntableId) const;

  std::vector<std::shared_ptr<IListItem>> listItems;
  int focusedIndex = -1;
  std::shared_ptr<TurntableListItem> getItemByTurntableId(int TurntableId);
  std::shared_ptr<TurntableIndexListItem> getIndexItemByTurntableAndIndex(int turntableId, int indexId);

  lv_msg_sub_dsc_t *turntable_changed_sub = nullptr;
  lv_timer_t *turntable_flash_timer = nullptr;
  int flashingTurntableId = -1;
  int flashingIndexId = -1;
  bool flashCheckedState = false;

  bool isCleanedUp = false;
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *list_Turntables = nullptr;
  lv_obj_t *btn_back = nullptr;
  lv_obj_t *currentButton = nullptr;

protected:
  TurntableListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurntableListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<TurntableListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }

  static void flash_timer_trampoline(lv_timer_t *timer) {
    auto *self = static_cast<TurntableListScreen *>(lv_timer_get_user_data(timer));
    if (self)
      self->onFlashTimerTick();
  }
};
} // namespace display
