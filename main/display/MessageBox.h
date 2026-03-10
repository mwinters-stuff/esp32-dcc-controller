#pragma once

#include <lvgl.h>

namespace display {

enum class MessageBoxState {
  Info,
  Success,
  Warning,
  Error,
};

using MessageBoxOkCallback = void (*)(void *userData);

void showMessageBox(const char *title, const char *message, MessageBoxState state, MessageBoxOkCallback onOk = nullptr,
                    void *onOkUserData = nullptr);

} // namespace display
