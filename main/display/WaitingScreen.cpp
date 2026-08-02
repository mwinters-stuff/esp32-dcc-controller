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

  // Spinner sits in the upper half of the screen.
  spinner = makeSpinner(lvObj_, 0, -60, 40, 1000);

  // A flex-column container stacks the two labels vertically so the sub-label
  // always sits below the title even when the title wraps to multiple lines.
  lv_obj_t *label_container = lv_obj_create(lvObj_);
  lv_obj_set_style_bg_opa(label_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(label_container, 0, 0);
  lv_obj_set_style_outline_width(label_container, 0, 0);
  lv_obj_set_style_pad_all(label_container, 0, 0);
  lv_obj_set_style_pad_gap(label_container, 10, 0);
  lv_obj_clear_flag(label_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(label_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(label_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_size(label_container, lv_pct(90), LV_SIZE_CONTENT);
  lv_obj_align(label_container, LV_ALIGN_CENTER, 0, 40);

  label = lv_label_create(label_container);
  lv_label_set_text(label, message.c_str());
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, lv_pct(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  setStyle(label, "label.title");

  sub_label = lv_label_create(label_container);
  lv_label_set_text(sub_label, subMessage.c_str());
  lv_label_set_long_mode(sub_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sub_label, lv_pct(100));
  lv_obj_set_style_text_align(sub_label, LV_TEXT_ALIGN_CENTER, 0);
  setStyle(sub_label, "label.main");

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
