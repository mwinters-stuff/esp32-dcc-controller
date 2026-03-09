#include "WaitingScreen.h"
#include "LvglWrapper.h"
#include "definitions.h"

namespace display {
using namespace ui;

void WaitingScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;
  lv_obj_clean(lv_scr_act());
  lvObj_ = lv_scr_act();
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
        if (self->isCleanedUp)
          return;
        self->setLabel("Connection Failed");

        // Stop and delete any previous failure timer before creating a new one
        if (self->failure_timer) {
          esp_timer_stop(self->failure_timer);
          esp_timer_delete(self->failure_timer);
          self->failure_timer = nullptr;
        }

        // 5-second timer to return to parent screen
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
        esp_timer_create(&timer_args, &self->failure_timer);
        esp_timer_start_once(self->failure_timer, 5000000); // 5 seconds in microseconds
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
  if (msg_subscribe_success) {
    lv_msg_unsubscribe(msg_subscribe_success);
    msg_subscribe_success = nullptr;
  }
  if (msg_subscribe_failed) {
    lv_msg_unsubscribe(msg_subscribe_failed);
    msg_subscribe_failed = nullptr;
  }
  if (failure_timer) {
    esp_timer_stop(failure_timer);
    esp_timer_delete(failure_timer);
    failure_timer = nullptr;
  }
  isCleanedUp = true;

  if (label) {
    lv_obj_clean(label);
    label = nullptr;
  }
  if (sub_label) {
    lv_obj_clean(sub_label);
    sub_label = nullptr;
  }
  if (spinner) {
    lv_obj_clean(spinner);
    spinner = nullptr;
  }
  // lv_obj_clean(lvObj_);
}

} // namespace display
