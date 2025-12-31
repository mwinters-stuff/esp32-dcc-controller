#pragma once
#include "Screen.h"
#include <memory>

#include "DCCConnectListItem.h"

namespace display {
class ConnectDCCScreen : public Screen, public std::enable_shared_from_this<ConnectDCCScreen> {
public:
  static std::shared_ptr<ConnectDCCScreen> instance() {
    static std::shared_ptr<ConnectDCCScreen> s;
    if (!s)
      s.reset(new ConnectDCCScreen());
    return s;
  }
  ConnectDCCScreen(const ConnectDCCScreen &) = delete;
  ConnectDCCScreen &operator=(const ConnectDCCScreen &) = delete;
  ~ConnectDCCScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  void connectToDCCServer(std::shared_ptr<DCCConnectListItem> dccItem);

  void refreshMdnsList();
  void refreshSavedList();
  void resetMsgHandlers();
  std::shared_ptr<DCCConnectListItem> getItem(lv_obj_t *bn);
  
    void button_connect_callback(lv_event_t *e);
  void button_save_callback(lv_event_t *e);
  void button_back_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);
private:
  std::vector<std::shared_ptr<DCCConnectListItem>> detectedListItems;
  std::shared_ptr<DCCConnectListItem> savedListItem;
  

  void *mdns_added_sub = nullptr;
  void *mdns_changed_sub = nullptr;
  void *subscribe_failed = nullptr;

  lv_obj_t *lbl_title;
  // lv_obj_t *tab_view;
  lv_obj_t *list_auto;
  // lv_obj_t *list_saved;
  lv_obj_t *btn_back;
  lv_obj_t *btn_save;
  lv_obj_t *btn_connect;
  lv_obj_t *currentButton = nullptr;


protected:
  ConnectDCCScreen() = default;

  static void event_connect_trampoline(lv_event_t *e) {
    auto *self = static_cast<ConnectDCCScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_connect_callback(e);
  }

  static void event_save_trampoline(lv_event_t *e) {
    auto *self = static_cast<ConnectDCCScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_save_callback(e);
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<ConnectDCCScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_callback(e);
  }


  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<ConnectDCCScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }


};
} // namespace display
