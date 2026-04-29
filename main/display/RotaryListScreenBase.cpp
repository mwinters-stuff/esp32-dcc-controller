#include "RotaryListScreenBase.h"
#include "utilities/RotaryEncoder.h"
#include <lvgl.h>

namespace display {

void RotaryListScreenBase::applyFocusOutline(lv_obj_t *obj, bool focused) {
  if (!obj) {
    return;
  }
  lv_obj_set_style_border_width(obj, focused ? 2 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_opa(obj, focused ? LV_OPA_100 : LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_color(obj, lv_color_hex(0xFF6B00), LV_PART_MAIN);
  lv_obj_set_style_border_side(obj, focused ? LV_BORDER_SIDE_FULL : LV_BORDER_SIDE_NONE, LV_PART_MAIN);
}

void RotaryListScreenBase::rotaryAttach() {
  pendingRotateSteps_.store(0, std::memory_order_relaxed);
  utilities::RotaryEncoder::instance()->setCallbacks(
      &RotaryListScreenBase::rotary_rotate_trampoline, &RotaryListScreenBase::rotary_click_trampoline,
      &RotaryListScreenBase::rotary_long_press_trampoline, this, &RotaryListScreenBase::rotary_double_click_trampoline);
}

void RotaryListScreenBase::rotaryDetach() {
  pendingRotateSteps_.store(0, std::memory_order_relaxed);
  utilities::RotaryEncoder::instance()->clearCallbacks(this);
}

void RotaryListScreenBase::rotaryNavigateBack() {
  if (!rotaryInputEnabled()) {
    return;
  }
  if (auto screen = parentScreen_.lock()) {
    cleanUp();
    screen->showScreen();
  }
}

void RotaryListScreenBase::rotaryHandleLongPress() { rotaryNavigateBack(); }

void RotaryListScreenBase::processPendingRotate() {
  if (!rotaryInputEnabled()) {
    pendingRotateSteps_.store(0, std::memory_order_relaxed);
    return;
  }

  int32_t steps = pendingRotateSteps_.exchange(0, std::memory_order_relaxed);
  while (steps > 0) {
    rotaryMoveFocus(1);
    --steps;
  }
  while (steps < 0) {
    rotaryMoveFocus(-1);
    ++steps;
  }
}

void RotaryListScreenBase::rotary_rotate_trampoline(int32_t delta, void *userData) {
  auto *self = static_cast<RotaryListScreenBase *>(userData);
  if (!self || !self->rotaryInputEnabled()) {
    return;
  }

  self->pendingRotateSteps_.fetch_add(delta, std::memory_order_relaxed);
  lv_async_call(&RotaryListScreenBase::rotary_process_trampoline, self);
}

void RotaryListScreenBase::rotary_click_trampoline(void *userData) {
  auto *self = static_cast<RotaryListScreenBase *>(userData);
  if (!self || !self->rotaryInputEnabled()) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<RotaryListScreenBase *>(ctx);
        if (screen && screen->rotaryInputEnabled()) {
          screen->rotaryActivateFocused();
        }
      },
      self);
}

void RotaryListScreenBase::rotary_double_click_trampoline(void *userData) {
  auto *self = static_cast<RotaryListScreenBase *>(userData);
  if (!self || !self->rotaryInputEnabled()) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<RotaryListScreenBase *>(ctx);
        if (screen && screen->rotaryInputEnabled()) {
          screen->rotaryHandleDoubleClick();
        }
      },
      self);
}

void RotaryListScreenBase::rotary_long_press_trampoline(void *userData) {
  auto *self = static_cast<RotaryListScreenBase *>(userData);
  if (!self || !self->rotaryInputEnabled()) {
    return;
  }

  lv_async_call(
      [](void *ctx) {
        auto *screen = static_cast<RotaryListScreenBase *>(ctx);
        if (screen && screen->rotaryInputEnabled()) {
          screen->rotaryHandleLongPress();
        }
      },
      self);
}

void RotaryListScreenBase::rotary_process_trampoline(void *userData) {
  auto *self = static_cast<RotaryListScreenBase *>(userData);
  if (self) {
    self->processPendingRotate();
  }
}

} // namespace display