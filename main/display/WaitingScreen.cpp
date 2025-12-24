#include "WaitingScreen.h"
#include "definitions.h"
#include "LvglWrapper.h"

namespace display {
using namespace ui;

void WaitingScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {

  spinner = makeSpinner(lvObj_, 0, 0, 40, 1000);
  label = makeLabel(lvObj_, message.c_str(), LV_ALIGN_TOP_MID, 0, 100, "label.title");
  sub_label = makeLabel(lvObj_, subMessage.c_str(), LV_ALIGN_TOP_MID, 0, 140, "label.main");

  msg_subscribe_success = lv_msg_subscribe(
      MSG_DCC_CONNECTION_SUCCESS,
      [](void *s, lv_msg_t *msg) {
        auto self = static_cast<WaitingScreen *>(msg->user_data);

        if (auto parent = self->parentScreen_.lock()) {
          self->unsubscribeAll();
          parent->showScreen();
        }
      },
      this);

  msg_subscribe_failed = lv_msg_subscribe(
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
                                                      screen->unsubscribeAll();
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
  if (label) {
    lv_label_set_text(label, text.c_str());
  }
}

void WaitingScreen::setSubLabel(const std::string &text) {
  subMessage = text;
  if (sub_label) {
    lv_label_set_text(sub_label, text.c_str());
  }
}

void WaitingScreen::unsubscribeAll() {
  lv_msg_unsubscribe(msg_subscribe_success);
  lv_msg_unsubscribe(msg_subscribe_failed);
}

} // namespace display
