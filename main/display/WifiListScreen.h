#pragma once
#include <esp_wifi.h>
#include <memory>
#include <utility>
#include <vector>
#include "WifiListItem.h"

#include "Screen.h"

namespace display {
using namespace ui;

class WifiListScreen : public Screen, public std::enable_shared_from_this<WifiListScreen> {
public:
  static std::shared_ptr<WifiListScreen> instance() {
    static std::shared_ptr<WifiListScreen> s;
    if (!s)
      s.reset(new WifiListScreen());
    return s;
  }
  ~WifiListScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  // Optionally return the list object to load screen

  WifiListScreen(const WifiListScreen &) = delete;
  WifiListScreen &operator=(const WifiListScreen &) = delete;

  void button_connect_event_callback(lv_event_t *e);
  void button_back_event_callback(lv_event_t *e);
  void button_listitem_click_event_callback(lv_event_t *e);

protected:
  WifiListScreen() = default;

  static void event_connect_trampoline(lv_event_t *e) {
    auto *self = static_cast<WifiListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_connect_event_callback(e);
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<WifiListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_event_callback(e);
  }

  static void event_listitem_click_trampoline(lv_event_t *e) {
    auto *self = static_cast<WifiListScreen *>(lv_event_get_user_data(e));
    if (self)
      self->button_listitem_click_event_callback(e);
  }

private:
  TaskHandle_t scanTaskHandle = 0;
  lv_obj_t *_lv_obj = nullptr;
  lv_obj_t * lbl_title;
  lv_obj_t * btn_back;
  lv_obj_t * btn_connect;
  lv_obj_t * list_view;
  lv_obj_t * spinner;
  lv_obj_t * currentButton = nullptr;

  std::vector<std::shared_ptr<WifiListItem>> items;

  void scanWifiTask();
  void populateList(const std::vector<wifi_ap_record_t> &records);
  void disableButtons();
  void enableButtons();
  std::shared_ptr<WifiListItem> getCurrentCheckedItem(lv_obj_t *bn);
};

} // namespace display
