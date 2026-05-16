#pragma once
#include "RotaryListScreenBase.h"
#include <memory>

#include "RosterListItem.h"

namespace display {
class RosterListScreen : public RotaryListScreenBase, public std::enable_shared_from_this<RosterListScreen> {
public:
  static std::shared_ptr<RosterListScreen> instance() {
    static std::shared_ptr<RosterListScreen> s;
    if (!s)
      s.reset(new RosterListScreen());
    return s;
  }
  RosterListScreen(const RosterListScreen &) = delete;
  RosterListScreen &operator=(const RosterListScreen &) = delete;
  ~RosterListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void refreshList();
  void unsubscribeAll();
  std::shared_ptr<RosterListItem> getItem(lv_obj_t *bn);
  std::shared_ptr<RosterListItem> getItemByAddress(int address);

  void button_back_callback(lv_event_t *e);

private:
  std::vector<std::shared_ptr<RosterListItem>> listItems;
  int focusedIndex = -1;
  int expandedAddress = -1;

  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override;
  void rotaryActivateFocused() override;

  void moveFocus(int direction);
  void updateFocusedState();
  void updateExpandedState();
  void selectAddress(int address);
  void applyLocoState(int address, int speed, Direction direction, int functionMap);
  void requestThrottle(int address, int speed, Direction direction);
  void requestStop(int address);
  void requestFunction(int address, int function, bool on);

  lv_msg_sub_dsc_t *roster_received_sub = nullptr;
  lv_msg_sub_dsc_t *loco_changed_sub = nullptr;

  bool isCleanedUp = false;
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *list_roster = nullptr;
  lv_obj_t *btn_back = nullptr;

protected:
  RosterListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }
};
} // namespace display
