#include "Screen.h"
#include "ui/lv_msg.h"
// #include "ui/LvglButton.h"
// #include "ui/LvglButtonSymbol.h"
// #include "ui/LvglHorizontalLayout.h"
// #include "ui/LvglKeyboard.h"
// #include "ui/LvglLabel.h"
// #include "ui/LvglSpinner.h"
// #include "ui/LvglTextArea.h"
// #include "ui/LvglVerticalLayout.h"
#include "utilities/WifiHandler.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <string>

namespace display {
using namespace ui;

class WifiConnectScreen : public Screen, public std::enable_shared_from_this<WifiConnectScreen> {
  bool isCleanedUp = false;

public:
  static std::shared_ptr<WifiConnectScreen> instance() {
    static std::shared_ptr<WifiConnectScreen> s;
    if (!s)
      s.reset(new WifiConnectScreen());
    return s;
  }
  ~WifiConnectScreen() override = default;

  void setSSID(const std::string &ssid);
  void unsubscribeAll();

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  WifiConnectScreen(const WifiConnectScreen &) = delete;
  WifiConnectScreen &operator=(const WifiConnectScreen &) = delete;

  lv_obj_t *lbl_status = nullptr;
  lv_obj_t *lbl_spinner = nullptr;
  lv_obj_t *kb_keyboard = nullptr;

  void wifi_connected_callback(lv_msg_t *msg);
  void wifi_failed_callback(lv_msg_t *msg);

  void event_password_show_callback(lv_event_t *e);
  void event_keyboard_callback(lv_event_t *e);

private:
  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *lbl_subtitle = nullptr;
  lv_obj_t *vert_container = nullptr;
  lv_obj_t *lbl_pwd = nullptr;
  lv_obj_t *pwd_container = nullptr;
  lv_obj_t *ta_password = nullptr;
  lv_obj_t *bs_password_show = nullptr;

  std::string ssid;

  void createScreen();

  lv_msg_sub_dsc_t *wifi_connected_sub = nullptr;
  lv_msg_sub_dsc_t *wifi_failed_sub = nullptr;

protected:
  WifiConnectScreen() = default;

  static void wifi_connected_trampoline(lv_msg_t *msg) {
    auto *self = static_cast<WifiConnectScreen *>(lv_msg_get_user_data(msg));
    if (self)
      self->wifi_connected_callback(msg);
  }

  static void wifi_failed_trampoline(lv_msg_t *msg) {
    auto *self = static_cast<WifiConnectScreen *>(lv_msg_get_user_data(msg));
    if (self)
      self->wifi_failed_callback(msg);
  }

  static void event_password_show_trampoline(lv_event_t *e) {
    auto *self = static_cast<WifiConnectScreen *>(lv_event_get_user_data(e));
    if (self)
      self->event_password_show_callback(e);
  }

  static void event_keyboard_trampoline(lv_event_t *e) {
    auto *self = static_cast<WifiConnectScreen *>(lv_event_get_user_data(e));
    if (self)
      self->event_keyboard_callback(e);
  }
};
} // namespace display