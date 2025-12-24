#include "Screen.h"
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

  lv_obj_t *lbl_status;
  lv_obj_t *lbl_spinner;
  lv_obj_t *kb_keyboard;

  void wifi_connected_callback(void *s, lv_msg_t *msg);
  void wifi_failed_callback(void *s, lv_msg_t *msg);

  void event_password_show_callback(lv_event_t *e);
  void event_keyboard_callback(lv_event_t *e);
  
private:
  lv_obj_t *lbl_title;
  lv_obj_t *lbl_subtitle;
  lv_obj_t *vert_container;
  lv_obj_t *lbl_pwd;
  lv_obj_t *pwd_container;
  lv_obj_t *ta_password;
  lv_obj_t *bs_password_show;

  std::string ssid;

  void createScreen();

  void *wifi_connected_sub = nullptr;
  void *wifi_failed_sub = nullptr;

protected:
  WifiConnectScreen() = default;

  static void wifi_connected_trampoline(void *s, lv_msg_t *msg){
    auto *self = static_cast<WifiConnectScreen *>(msg->user_data);
    if (self)
      self->wifi_connected_callback(s, msg);
  }

  static void wifi_failed_trampoline(void *s, lv_msg_t *msg){
    auto *self = static_cast<WifiConnectScreen *>(msg->user_data);
    if (self)
      self->wifi_failed_callback(s, msg);
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