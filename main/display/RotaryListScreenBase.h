#pragma once

#include "Screen.h"
#include <atomic>
#include <cstdint>

namespace display {

class RotaryListScreenBase : public Screen {
protected:
  void rotaryAttach();
  void rotaryDetach();
  void rotaryNavigateBack();

  virtual bool rotaryInputEnabled() const = 0;
  virtual void rotaryMoveFocus(int direction) = 0;
  virtual void rotaryActivateFocused() = 0;
  virtual void rotaryHandleLongPress();
  virtual void rotaryHandleDoubleClick() {}

  static void applyFocusOutline(lv_obj_t *obj, bool focused);

private:
  void processPendingRotate();

  static void rotary_rotate_trampoline(int32_t delta, void *userData);
  static void rotary_click_trampoline(void *userData);
  static void rotary_double_click_trampoline(void *userData);
  static void rotary_long_press_trampoline(void *userData);
  static void rotary_process_trampoline(void *userData);

  std::atomic<int32_t> pendingRotateSteps_{0};
};

} // namespace display