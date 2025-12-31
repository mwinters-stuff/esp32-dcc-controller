#pragma once
#include "Screen.h"
#include <memory>

#include "TurnoutListItem.h"

namespace display {
class TurnoutListScreen : public Screen, public std::enable_shared_from_this<TurnoutListScreen> {
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
  void resetMsgHandlers();
  std::shared_ptr<TurnoutListItem> getItem(lv_obj_t *bn);
  
  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);
private:
  std::vector<std::shared_ptr<TurnoutListItem>> listItems;
  

 void *subscribe_failed = nullptr;

  lv_obj_t *lbl_title;
  lv_obj_t *list_auto;
  lv_obj_t *btn_back;
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
