#include "DisplayManager.h"

LGFX DisplayManager::gfx;

// --- LVGL Display Driver Callback ---
void DisplayManager::disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() == 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1, (lgfx::rgb565_t *)color_p);
  lv_disp_flush_ready(disp);
}
