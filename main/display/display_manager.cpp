#include "display_manager.h"

LGFX DisplayManager::gfx;

// --- LVGL Display Driver Callback ---
void DisplayManager::disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
  if (gfx.getStartCount() == 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1, (lgfx::rgb565_t *)px_map);
  lv_disp_flush_ready(disp);
}
