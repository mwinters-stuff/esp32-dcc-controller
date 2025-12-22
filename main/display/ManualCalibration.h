#pragma once
#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <lvgl.h>
#include <memory>

namespace display {

enum calibrateState { notCalibrated, calibrate, showScreen, calibrated };

class ManualCalibration : public Screen {
public:
  // Get the singleton instance. Optional parent only used on first creation.
  static std::shared_ptr<ManualCalibration> instance() {
    static std::shared_ptr<ManualCalibration> s;
    if (!s)
      s.reset(new ManualCalibration());
    return s;
  }

  virtual ~ManualCalibration() = default;

  // existing public API
  virtual void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  calibrateState loadCalibrationFromNVS();
  virtual void cleanUp() override;
  void calibrate();

  // non-copyable, non-movable
  ManualCalibration(const ManualCalibration &) = delete;
  ManualCalibration &operator=(const ManualCalibration &) = delete;
  ManualCalibration &operator=(ManualCalibration &&) = delete;

  void button_start_event_callback(lv_event_t *e);
  void button_back_event_callback(lv_event_t *e);

protected:
  ManualCalibration() {}; // protected ctor

  static void event_start_trampoline(lv_event_t *e) {
    auto *self = static_cast<ManualCalibration *>(lv_event_get_user_data(e));
    if (self)
      self->button_start_event_callback(e);
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<ManualCalibration *>(lv_event_get_user_data(e));
    if (self)
      self->button_back_event_callback(e);
  }

private:
  lv_obj_t *lbl_title;
  lv_obj_t *lbl_sub_title;
  lv_obj_t *btn_start;
  lv_obj_t *btn_back;

  uint16_t parameters[8];

  void rebootToCalibrate();
  void save_to_nvs(calibrateState state);
  calibrateState load_from_nvs();
};

} // namespace display
