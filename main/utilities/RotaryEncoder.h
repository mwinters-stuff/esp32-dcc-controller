#pragma once

#include <cstdint>
#include <memory>

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace utilities {

class RotaryEncoder {
public:
  using RotateCallback = void (*)(int32_t delta, void *userData);
  using ClickCallback = void (*)(void *userData);
  using DoubleClickCallback = void (*)(void *userData);
  using LongPressCallback = void (*)(void *userData);
  using ActivityCallback = void (*)(void *userData);

  static std::shared_ptr<RotaryEncoder> instance() {
    static std::shared_ptr<RotaryEncoder> s;
    if (!s)
      s.reset(new RotaryEncoder());
    return s;
  }

  RotaryEncoder(const RotaryEncoder &) = delete;
  RotaryEncoder &operator=(const RotaryEncoder &) = delete;
  ~RotaryEncoder();

  bool init(gpio_num_t gpioA, gpio_num_t gpioB, bool reverseDirection = false, bool enableSwitch = false,
            gpio_num_t gpioSw = GPIO_NUM_NC, uint8_t switchActiveLevel = 0);
  void deinit();
  void setCallbacks(RotateCallback rotateCb, ClickCallback clickCb, LongPressCallback longPressCb, void *userData,
                    DoubleClickCallback doubleClickCb = nullptr);
  void clearCallbacks(void *userData = nullptr);
  void setActivityCallback(ActivityCallback cb, void *userData);
  bool isInitialized() const { return initialized_; }

private:
  RotaryEncoder() = default;

  static void IRAM_ATTR encoder_isr_handler(void *arg);
  static void monitor_task_trampoline(void *arg);
  static void sw_single_click_trampoline(void *button_handle, void *usr_data);
  static void sw_double_click_trampoline(void *button_handle, void *usr_data);
  static void sw_long_press_trampoline(void *button_handle, void *usr_data);
  void emitRotate(int32_t delta);
  void emitClick();
  void emitDoubleClick();
  void emitLongPress();

  void *buttonHandle_ = nullptr;
  gpio_num_t gpioA_ = GPIO_NUM_NC;
  gpio_num_t gpioB_ = GPIO_NUM_NC;
  bool reverseDirection_ = false;
  volatile int32_t count_ = 0;
  volatile uint8_t prevState_ = 0;
  TaskHandle_t monitorTask_ = nullptr;
  portMUX_TYPE countMux_ = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE callbackMux_ = portMUX_INITIALIZER_UNLOCKED;
  RotateCallback rotateCallback_ = nullptr;
  ClickCallback clickCallback_ = nullptr;
  DoubleClickCallback doubleClickCallback_ = nullptr;
  LongPressCallback longPressCallback_ = nullptr;
  void *callbackUserData_ = nullptr;
  ActivityCallback activityCallback_ = nullptr;
  void *activityUserData_ = nullptr;
  bool initialized_ = false;
};

} // namespace utilities
