#pragma once
#include "Screen.h"
#include <lvgl.h>
#include <memory>
#include <string>

#include "ui/LvglLabel.h"
#include "ui/LvglSpinner.h"

namespace display {

class WaitingScreen : public Screen {
public:
  static std::shared_ptr<WaitingScreen> instance() {
    static std::shared_ptr<WaitingScreen> s;
    if (!s)
      s.reset(new WaitingScreen());
    return s;
  }

  WaitingScreen(const WaitingScreen &) = delete;
  WaitingScreen &operator=(const WaitingScreen &) = delete;
  ~WaitingScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void setLabel(const std::string &text);
  void setSubLabel(const std::string &text);

private:
  std::string message;
  std::string subMessage;
  std::unique_ptr<ui::LvglLabel> label_;
  std::unique_ptr<ui::LvglLabel> sub_label_;
  std::unique_ptr<ui::LvglSpinner> spinner_;

protected:
  WaitingScreen() = default;
};

} // namespace display