/**
 * @file DisplayManager.cpp
 * @brief LVGL display driver bridge for the LovyanGFX-backed ILI9488 panel.
 *
 * Owns the global LGFX instance and provides the flush callback that LVGL
 * calls after rendering each dirty rectangle.
 */
#include "DisplayManager.h"
#include <esp_log.h>

LGFX DisplayManager::gfx;
static const char *TAG = "DISPLAY_MANAGER";

// LVGL flush callback: pushes a rendered rectangle to the display via
// LovyanGFX DMA. Begins a write transaction on the first call of a frame and
// signals LVGL that the flush is complete so it can continue rendering.
void DisplayManager::disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
  // LovyanGFX async DMA flush pattern (matches LovyanGFX LVGL example).
  if (gfx.getStartCount() == 0) {
    gfx.startWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (lgfx::rgb565_t *)color_p);

  lv_display_flush_ready(disp);
}
