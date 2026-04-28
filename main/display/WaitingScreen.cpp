/**
 * @file WaitingScreen.cpp
 * @brief Intermediate screen shown while a DCC-EX TCP connection is in progress.
 *
 * Displays a spinner and status text while the connection task runs. Subscribes
 * to MSG_DCC_CONNECTION_FAILED to show an error message box and return to the
 * main screen if the attempt fails.
 */
#include "WaitingScreen.h"
#include "FirstScreen.h"
#include "LvglWrapper.h"
#include "MessageBox.h"
#include "definitions.h"

namespace display {
using namespace ui;

namespace {
// LVGL async callback: returns to FirstScreen after an error is dismissed.
void return_to_main_screen(void *) { display::FirstScreen::instance()->showScreen(); }
} // namespace

// Builds the waiting screen UI (spinner + labels) and subscribes to the
// connection-failed message.
void WaitingScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  isCleanedUp = false;
  lv_obj_clean(lv_screen_active());
  lvObj_ = lv_screen_active();
  spinner = makeSpinner(lvObj_, 0, 0, 40, 1000);
  label = makeLabel(lvObj_, message.c_str(), LV_ALIGN_TOP_MID, 0, 100, "label.title");
  sub_label = makeLabel(lvObj_, subMessage.c_str(), LV_ALIGN_TOP_MID, 0, 140, "label.main");

  msg_subscribe_success = lv_msg_subscribe(
      MSG_DCC_CONNECTION_SUCCESS,
      [](lv_msg_t *msg) {
        auto self = static_cast<WaitingScreen *>(lv_msg_get_user_data(msg));

        if (auto parent = self->parentScreen_.lock()) {
          self->unsubscribeAll();
          parent->showScreen();
        }
      },
      this);

  msg_subscribe_failed = lv_msg_subscribe(
      MSG_DCC_CONNECTION_FAILED,
      [](lv_msg_t *msg) {
        auto self = static_cast<WaitingScreen *>(lv_msg_get_user_data(msg));
        if (self->isCleanedUp)
          return;
        self->unsubscribeAll();
        display::showMessageBox("Connection Failed", "Could not connect to DCC server.",
                                display::MessageBoxState::Error, return_to_main_screen, nullptr);
      },
      this);
}

// Updates the primary status label text.
void WaitingScreen::setLabel(const std::string &text) {
  message = text;
  if (label) {
    lv_label_set_text(label, text.c_str());
  }
}

// Updates the secondary sub-label text shown below the status label.
void WaitingScreen::setSubLabel(const std::string &text) {
  subMessage = text;
  if (sub_label) {
    lv_label_set_text(sub_label, text.c_str());
  }
}

// Removes message subscriptions.
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
