#pragma once

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <atomic>
#include "../main/LGFX_ILI9488_S3.hpp"

class DisplayManager {
public:
    static uint16_t bufferSize(){
        return gfx.screenWidth * gfx.screenHeight / 10;
    };

    static LGFX gfx;
    static std::atomic<bool> calibrating;
    static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
};
