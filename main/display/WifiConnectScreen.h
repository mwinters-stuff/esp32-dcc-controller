#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglButtonSymbol.h"
#include "ui/LvglHorizontalLayout.h"
#include "ui/LvglKeyboard.h"
#include "ui/LvglLabel.h"
#include "ui/LvglSpinner.h"
#include "ui/LvglTextArea.h"
#include "ui/LvglVerticalLayout.h"
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

  std::unique_ptr<LvglLabel> lbl_status_;
  std::unique_ptr<LvglSpinner> lbl_spinner_;
  std::unique_ptr<LvglKeyboard> kb_keyboard_;

private:
  std::unique_ptr<LvglLabel> lbl_title_;
  std::unique_ptr<LvglLabel> lbl_subtitle_;
  std::unique_ptr<LvglVerticalLayout> vert_container_;
  std::unique_ptr<LvglLabel> lbl_pwd_;
  std::unique_ptr<LvglHorizontalLayout> pwd_container_;
  std::unique_ptr<LvglTextArea> ta_password_;
  std::unique_ptr<LvglButtonSymbol> bs_password_show_;

  std::string ssid_;

  void createScreen();

  void *wifi_connected_sub_ = nullptr;
  void *wifi_failed_sub_ = nullptr;

protected:
  WifiConnectScreen() = default;
};
} // namespace display