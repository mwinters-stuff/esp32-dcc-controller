#include "WaitingScreen.h"
#include "definitions.h"

namespace display {
using namespace ui;

void WaitingScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {

  spinner_ = std::make_unique<LvglSpinner>(lvObj_);
  label_ = std::make_unique<LvglLabel>(lvObj_, message, LV_ALIGN_TOP_MID, 0, 100);
  label_->setStyle("label.title");
  sub_label_ = std::make_unique<LvglLabel>(lvObj_, subMessage, LV_ALIGN_TOP_MID, 0, 140);
  sub_label_->setStyle("label.main");

  lv_msg_subscribe(
      MSG_DCC_CONNECTION_SUCCESS,
      [](void *s, lv_msg_t *msg) {
        auto screen = WaitingScreen::instance();
        if (auto parent = screen->parentScreen_.lock()) {
          parent->showScreen();
        }
      },
      this);

  lv_msg_subscribe(
      MSG_DCC_CONNECTION_FAILED,
      [](void *s, lv_msg_t *msg) {
        auto self = static_cast<WaitingScreen *>(msg->user_data);
        self->setLabel("Connection Failed");

        // 5-second timer to return to parent screen
        esp_timer_handle_t timer_handle = nullptr;
        esp_timer_create_args_t timer_args = {.callback =
                                                  [](void *arg) {
                                                    auto screen = static_cast<WaitingScreen *>(arg);
                                                    if (auto parent = screen->parentScreen_.lock()) {
                                                      parent->showScreen();
                                                    }
                                                  },
                                              .arg = self,
                                              .dispatch_method = ESP_TIMER_TASK,
                                              .name = "waiting_screen_timer",
                                              .skip_unhandled_events = false};
        esp_timer_create(&timer_args, &timer_handle);
        esp_timer_start_once(timer_handle, 5000000); // 5 seconds in microseconds
      },
      this);
}

void WaitingScreen::setLabel(const std::string &text) {
  message = text;
  if (label_) {
    label_->setText(text);
  }
}

void WaitingScreen::setSubLabel(const std::string &text) {
  subMessage = text;
  if (sub_label_) {
    sub_label_->setText(text);
  }
}

} // namespace display
