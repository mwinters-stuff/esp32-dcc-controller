#pragma once
#include "lvgl.h"

namespace display {

bool loadCalibrationFromNVS();
void startManualCalibration(lv_obj_t* parent);
void applyTouchTransform(int16_t* x, int16_t* y);
bool read_raw_touch(int16_t* x, int16_t* y);  // your hardware read


} // namespace display
