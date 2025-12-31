#pragma once
#include "Screen.h"
#include <memory>

#include "RosterListItem.h"

namespace display {
class RosterListScreen : public Screen, public std::enable_shared_from_this<RosterListScreen> {
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
  void resetMsgHandlers();
  std::shared_ptr<RosterListItem> getItem(lv_obj_t *bn);

  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

private:
  std::vector<std::shared_ptr<RosterListItem>> listItems;

  void *subscribe_failed = nullptr;

  lv_obj_t *lbl_title;
  lv_obj_t *list_roster;
  lv_obj_t *btn_back;
  lv_obj_t *currentButton = nullptr;

protected:
  RosterListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<RosterListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }
};
} // namespace display
