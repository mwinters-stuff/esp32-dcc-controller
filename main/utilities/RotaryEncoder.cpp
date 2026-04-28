/**
 * @file RotaryEncoder.cpp
 * @brief Quadrature rotary encoder driver with push-button support.
 *
 * Uses GPIO interrupts and a quadrature transition table to decode rotation
 * direction and count. Button events (single click, long press) are handled
 * by the esp-idf `iot_button` component. All callbacks are invoked from a
 * dedicated FreeRTOS monitor task so callers receive events on a known stack.
 */
#include "RotaryEncoder.h"

#include <button_gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <iot_button.h>

namespace {
// Quadrature transition table: index is (prev_state << 2) | current_state.
constexpr int8_t kQuadratureDelta[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
} // namespace

namespace utilities {

static const char *TAG = "RotaryEncoder";

// iot_button press-down raw callback; logs the event for diagnostics.
static void sw_press_down_cb(void *button_handle, void *usr_data) { ESP_LOGI(TAG, "SW_PRESS_DOWN"); }

// iot_button press-up raw callback; logs the event for diagnostics.
static void sw_press_up_cb(void *button_handle, void *usr_data) { ESP_LOGI(TAG, "SW_PRESS_UP"); }

// iot_button single-click trampoline: casts usr_data to RotaryEncoder and
// calls emitClick().
void RotaryEncoder::sw_single_click_trampoline(void *button_handle, void *usr_data) {
  ESP_LOGI(TAG, "SW_SINGLE_CLICK");
  auto *self = static_cast<RotaryEncoder *>(usr_data);
  if (self) {
    self->emitClick();
  }
}

// iot_button long-press trampoline: casts usr_data to RotaryEncoder and calls
// emitLongPress().
void RotaryEncoder::sw_long_press_trampoline(void *button_handle, void *usr_data) {
  ESP_LOGI(TAG, "SW_LONG_PRESS_START");
  auto *self = static_cast<RotaryEncoder *>(usr_data);
  if (self) {
    self->emitLongPress();
  }
}

// GPIO ISR handler: reads the A/B pin states, looks up the quadrature delta
// and increments the encoder's pending count. Placed in IRAM for low latency.
void IRAM_ATTR RotaryEncoder::encoder_isr_handler(void *arg) {
  auto *self = static_cast<RotaryEncoder *>(arg);
  const uint8_t a = static_cast<uint8_t>(gpio_get_level(self->gpioA_));
  const uint8_t b = static_cast<uint8_t>(gpio_get_level(self->gpioB_));
  const uint8_t currentState = static_cast<uint8_t>((a << 1) | b);
  const uint8_t transition = static_cast<uint8_t>((self->prevState_ << 2) | currentState);

  int8_t delta = kQuadratureDelta[transition];
  if (self->reverseDirection_) {
    delta = -delta;
  }

  if (delta != 0) {
    portENTER_CRITICAL_ISR(&self->countMux_);
    self->count_ += delta;
    portEXIT_CRITICAL_ISR(&self->countMux_);
  }

  self->prevState_ = currentState;
}

// FreeRTOS task that wakes on a notification from encoder_isr_handler and
// calls emitRotate() with the accumulated delta.
void RotaryEncoder::monitor_task_trampoline(void *arg) {
  auto *self = static_cast<RotaryEncoder *>(arg);
  int32_t lastCount = 0;
  int32_t subStepAccumulator = 0;
  constexpr int32_t kSubStepsPerDetent = 4;

  while (true) {
    int32_t currentCount = 0;
    portENTER_CRITICAL(&self->countMux_);
    currentCount = self->count_;
    portEXIT_CRITICAL(&self->countMux_);

    int32_t delta = currentCount - lastCount;
    if (delta != 0) {
      subStepAccumulator += delta;
      lastCount = currentCount;

      int32_t detentSteps = 0;
      while (subStepAccumulator >= kSubStepsPerDetent) {
        ++detentSteps;
        subStepAccumulator -= kSubStepsPerDetent;
      }
      while (subStepAccumulator <= -kSubStepsPerDetent) {
        --detentSteps;
        subStepAccumulator += kSubStepsPerDetent;
      }

      if (detentSteps != 0) {
        ESP_LOGI(TAG, "%s detents=%ld count=%ld", detentSteps > 0 ? "ROTARY_UP" : "ROTARY_DOWN",
                 static_cast<long>(detentSteps > 0 ? detentSteps : -detentSteps), static_cast<long>(currentCount));
        self->emitRotate(detentSteps);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Registers rotate, click, long-press and optional process callbacks for the
// given userData context. Replaces any previously registered set.
void RotaryEncoder::setCallbacks(RotateCallback rotateCb, ClickCallback clickCb, LongPressCallback longPressCb,
                                 void *userData) {
  portENTER_CRITICAL(&callbackMux_);
  rotateCallback_ = rotateCb;
  clickCallback_ = clickCb;
  longPressCallback_ = longPressCb;
  callbackUserData_ = userData;
  portEXIT_CRITICAL(&callbackMux_);
}

// Removes all callbacks registered under userData. Called by screens during
// cleanUp so stale pointers are not invoked after the screen is destroyed.
void RotaryEncoder::clearCallbacks(void *userData) {
  portENTER_CRITICAL(&callbackMux_);
  if (userData == nullptr || callbackUserData_ == userData) {
    rotateCallback_ = nullptr;
    clickCallback_ = nullptr;
    longPressCallback_ = nullptr;
    callbackUserData_ = nullptr;
  }
  portEXIT_CRITICAL(&callbackMux_);
}

// Sets the global activity callback invoked on any encoder event (rotation or
// button). Used by the display-sleep subsystem to wake the screen.
void RotaryEncoder::setActivityCallback(ActivityCallback cb, void *userData) {
  portENTER_CRITICAL(&callbackMux_);
  activityCallback_ = cb;
  activityUserData_ = userData;
  portEXIT_CRITICAL(&callbackMux_);
}

// Dispatches a rotation event with the given delta to all registered rotate
// callbacks, then fires the activity callback.
void RotaryEncoder::emitRotate(int32_t delta) {
  RotateCallback rotateCb = nullptr;
  void *userData = nullptr;
  ActivityCallback actCb = nullptr;
  void *actData = nullptr;

  portENTER_CRITICAL(&callbackMux_);
  rotateCb = rotateCallback_;
  userData = callbackUserData_;
  actCb = activityCallback_;
  actData = activityUserData_;
  portEXIT_CRITICAL(&callbackMux_);

  if (actCb != nullptr) {
    actCb(actData);
  }
  if (rotateCb != nullptr) {
    rotateCb(delta, userData);
  }
}

// Dispatches a click event to all registered click callbacks, then fires the
// activity callback.
void RotaryEncoder::emitClick() {
  ClickCallback clickCb = nullptr;
  void *userData = nullptr;
  ActivityCallback actCb = nullptr;
  void *actData = nullptr;

  portENTER_CRITICAL(&callbackMux_);
  clickCb = clickCallback_;
  userData = callbackUserData_;
  actCb = activityCallback_;
  actData = activityUserData_;
  portEXIT_CRITICAL(&callbackMux_);

  if (actCb != nullptr) {
    actCb(actData);
  }
  if (clickCb != nullptr) {
    clickCb(userData);
  }
}

// Dispatches a long-press event to all registered long-press callbacks, then
// fires the activity callback.
void RotaryEncoder::emitLongPress() {
  LongPressCallback longPressCb = nullptr;
  void *userData = nullptr;
  ActivityCallback actCb = nullptr;
  void *actData = nullptr;

  portENTER_CRITICAL(&callbackMux_);
  longPressCb = longPressCallback_;
  userData = callbackUserData_;
  actCb = activityCallback_;
  actData = activityUserData_;
  portEXIT_CRITICAL(&callbackMux_);

  if (actCb != nullptr) {
    actCb(actData);
  }
  if (longPressCb != nullptr) {
    longPressCb(userData);
  }
}

// Destructor: calls deinit() to free GPIO/ISR/task resources.
RotaryEncoder::~RotaryEncoder() { deinit(); }

// Configures GPIOs for encoder A/B (and optionally the switch), installs the
// ISR, creates the monitor task and registers iot_button callbacks.
bool RotaryEncoder::init(gpio_num_t gpioA, gpio_num_t gpioB, bool reverseDirection, bool enableSwitch,
                         gpio_num_t gpioSw, uint8_t switchActiveLevel) {
  if (initialized_) {
    ESP_LOGI(TAG, "Rotary encoder already initialized");
    return true;
  }

  if (gpioA == GPIO_NUM_NC || gpioB == GPIO_NUM_NC) {
    ESP_LOGE(TAG, "Invalid rotary GPIOs A=%d B=%d", static_cast<int>(gpioA), static_cast<int>(gpioB));
    return false;
  }

  gpioA_ = gpioA;
  gpioB_ = gpioB;
  reverseDirection_ = reverseDirection;

  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << static_cast<uint32_t>(gpioA_)) | (1ULL << static_cast<uint32_t>(gpioB_));
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  if (gpio_config(&io_conf) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure rotary GPIOs");
    return false;
  }

  const esp_err_t isrServiceErr = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  if (isrServiceErr != ESP_OK && isrServiceErr != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to install GPIO ISR service");
    return false;
  }

  const uint8_t a = static_cast<uint8_t>(gpio_get_level(gpioA_));
  const uint8_t b = static_cast<uint8_t>(gpio_get_level(gpioB_));
  prevState_ = static_cast<uint8_t>((a << 1) | b);
  count_ = 0;

  if (gpio_isr_handler_add(gpioA_, encoder_isr_handler, this) != ESP_OK ||
      gpio_isr_handler_add(gpioB_, encoder_isr_handler, this) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add rotary ISR handlers");
    gpio_isr_handler_remove(gpioA_);
    gpio_isr_handler_remove(gpioB_);
    return false;
  }

  if (xTaskCreate(monitor_task_trampoline, "rotary_monitor", 3072, this, tskIDLE_PRIORITY + 1, &monitorTask_) !=
      pdPASS) {
    ESP_LOGE(TAG, "Failed to create rotary monitor task");
    gpio_isr_handler_remove(gpioA_);
    gpio_isr_handler_remove(gpioB_);
    return false;
  }

  if (enableSwitch) {
    if (gpioSw == GPIO_NUM_NC) {
      ESP_LOGE(TAG, "SW button enabled but GPIO is not configured");
      vTaskDelete(monitorTask_);
      monitorTask_ = nullptr;
      gpio_isr_handler_remove(gpioA_);
      gpio_isr_handler_remove(gpioB_);
      return false;
    }

    const button_config_t btn_cfg = {};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = static_cast<int32_t>(gpioSw),
        .active_level = switchActiveLevel,
        .enable_power_save = false,
        .disable_pull = false,
    };

    button_handle_t buttonHandle = nullptr;
    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &buttonHandle) != ESP_OK || buttonHandle == nullptr) {
      ESP_LOGE(TAG, "Failed to create SW button on GPIO %d", static_cast<int>(gpioSw));
      vTaskDelete(monitorTask_);
      monitorTask_ = nullptr;
      gpio_isr_handler_remove(gpioA_);
      gpio_isr_handler_remove(gpioB_);
      return false;
    }

    if (iot_button_register_cb(buttonHandle, BUTTON_PRESS_DOWN, nullptr, sw_press_down_cb, nullptr) != ESP_OK ||
        iot_button_register_cb(buttonHandle, BUTTON_PRESS_UP, nullptr, sw_press_up_cb, nullptr) != ESP_OK ||
        iot_button_register_cb(buttonHandle, BUTTON_SINGLE_CLICK, nullptr, &RotaryEncoder::sw_single_click_trampoline,
                               this) != ESP_OK ||
        iot_button_register_cb(buttonHandle, BUTTON_LONG_PRESS_START, nullptr, &RotaryEncoder::sw_long_press_trampoline,
                               this) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register one or more SW button callbacks");
      iot_button_delete(buttonHandle);
      vTaskDelete(monitorTask_);
      monitorTask_ = nullptr;
      gpio_isr_handler_remove(gpioA_);
      gpio_isr_handler_remove(gpioB_);
      return false;
    }

    buttonHandle_ = buttonHandle;
    ESP_LOGI(TAG, "Rotary encoder SW initialized on GPIO %d (active_level=%u)", static_cast<int>(gpioSw),
             static_cast<unsigned>(switchActiveLevel));
  }

  initialized_ = true;
  ESP_LOGI(TAG, "Rotary encoder ISR decoder initialized on GPIO A=%d B=%d", static_cast<int>(gpioA),
           static_cast<int>(gpioB));
  return true;
}

// Removes the ISR, deletes the monitor task and releases button handles.
void RotaryEncoder::deinit() {
  if (!initialized_) {
    return;
  }

  if (buttonHandle_ != nullptr) {
    auto buttonHandle = static_cast<button_handle_t>(buttonHandle_);
    iot_button_unregister_cb(buttonHandle, BUTTON_PRESS_DOWN, nullptr);
    iot_button_unregister_cb(buttonHandle, BUTTON_PRESS_UP, nullptr);
    iot_button_unregister_cb(buttonHandle, BUTTON_SINGLE_CLICK, nullptr);
    iot_button_unregister_cb(buttonHandle, BUTTON_LONG_PRESS_START, nullptr);
    iot_button_delete(buttonHandle);
    buttonHandle_ = nullptr;
  }

  if (monitorTask_ != nullptr) {
    vTaskDelete(monitorTask_);
    monitorTask_ = nullptr;
  }

  if (gpioA_ != GPIO_NUM_NC) {
    gpio_isr_handler_remove(gpioA_);
  }
  if (gpioB_ != GPIO_NUM_NC) {
    gpio_isr_handler_remove(gpioB_);
  }

  gpioA_ = GPIO_NUM_NC;
  gpioB_ = GPIO_NUM_NC;
  clearCallbacks();
  initialized_ = false;
  ESP_LOGI(TAG, "Rotary encoder deinitialized");
}

} // namespace utilities
