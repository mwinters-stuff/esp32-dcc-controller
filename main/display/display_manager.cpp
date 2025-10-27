#include "display_manager.h"

LGFX DisplayManager::gfx;
std::atomic<bool> DisplayManager::calibrating{false};


// --- LVGL Display Driver Callback ---
void DisplayManager::disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
      // If we're calibrating, avoid DMA. Use blocking pushImage so calibration drawing isn't overwritten.
    if (DisplayManager::calibrating.load(std::memory_order_acquire)) {
        // Ensure we end any open fast-write session before blocking write
        if (gfx.getStartCount() > 0) {
            gfx.endWrite();
        }
        // Blocking push (no DMA). cast is OK because LGFX pushImage accepts rgb565 pointer
        gfx.pushImage(area->x1, area->y1, area->x2 - area->x1 + 1,
                      area->y2 - area->y1 + 1, (lgfx::rgb565_t *)color_p);
        lv_disp_flush_ready(disp);
        return;
    }
  
  if (gfx.getStartCount() == 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1, (lgfx::rgb565_t *)color_p);
  lv_disp_flush_ready(disp);
}
