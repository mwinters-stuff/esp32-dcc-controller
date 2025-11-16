#pragma once
#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <memory>

namespace display
{

  class FirstScreen : public Screen, public std::enable_shared_from_this<FirstScreen>
  {
  public:
    static std::shared_ptr<FirstScreen> instance()
    {
      static std::shared_ptr<FirstScreen> s;
      if (!s)
        s.reset(new FirstScreen());
      return s;
    }

    ~FirstScreen() override = default;

    void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
    void cleanUp() override;

    FirstScreen(const FirstScreen &) = delete;
    FirstScreen &operator=(const FirstScreen &) = delete;

    void enableButtons(bool enableConnect);
    void disableButtons();

    std::shared_ptr<ui::LvglLabel> lbl_status;
    std::shared_ptr<ui::LvglLabel> lbl_ip;

    static void* subscribe_connected;
    static void* subscribe_failed;
    static void* subscribe_not_saved;

  protected:
    FirstScreen() = default;

  private:
    std::shared_ptr<ui::LvglLabel> lbl_title;
    std::shared_ptr<ui::LvglButton> btn_connect;
    std::shared_ptr<ui::LvglButton> btn_wifi_scan;
    std::shared_ptr<ui::LvglButton> btn_cal;
  };

} // namespace display
