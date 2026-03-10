#pragma once

#include "../main/LGFX_ILI9488_S3.hpp"
#include <LovyanGFX.hpp>
#include <atomic>
#include <lvgl.h>

class DisplayManager {
public:
  static uint16_t bufferSize() { return gfx.screenWidth * gfx.screenHeight / 10; };

  static LGFX gfx;
  static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p);
};
