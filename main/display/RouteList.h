#pragma once
#include "RotaryListScreenBase.h"
#include <memory>

#include "RouteListItem.h"

namespace display {
class RouteListScreen : public RotaryListScreenBase, public std::enable_shared_from_this<RouteListScreen> {
public:
  static std::shared_ptr<RouteListScreen> instance() {
    static std::shared_ptr<RouteListScreen> s;
    if (!s)
      s.reset(new RouteListScreen());
    return s;
  }
  RouteListScreen(const RouteListScreen &) = delete;
  RouteListScreen &operator=(const RouteListScreen &) = delete;
  ~RouteListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void refreshList();
  void unsubscribeAll();
  std::shared_ptr<RouteListItem> getItem(lv_obj_t *bn);

  void button_back_callback(lv_event_t *e);
  void button_pause_resume_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

private:
  std::vector<std::shared_ptr<RouteListItem>> listItems;
  int focusedIndex = -1;
  int selectedRouteId = -1;

  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override { moveFocus(direction); }
  void rotaryActivateFocused() override { activateFocused(); }

  void updateFocusedState();
  void moveFocus(int direction);
  void activateFocused();
  void startRoute(std::shared_ptr<RouteListItem> item);
  void updateSelectedState();
  void updatePauseResumeButton();
  void setStatusText(const char *text);

  bool isCleanedUp = false;
  bool routesPaused = false;
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *lbl_status = nullptr;
  lv_obj_t *list_routes = nullptr;
  lv_obj_t *btn_back = nullptr;
  lv_obj_t *btn_pause_resume = nullptr;

protected:
  RouteListScreen() = default;

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<RouteListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }

  static void event_pause_resume_trampoline(lv_event_t *e) {
    auto *self = static_cast<RouteListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_pause_resume_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<RouteListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }
};
} // namespace display
