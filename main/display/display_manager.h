#pragma once

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <atomic>
#include "../main/LGFX_ILI9488_S3.hpp"

#define BUF_LINES 10

class DisplayManager {
public:
    static size_t bufferSize(){
        return LGFX::screenWidth * BUF_LINES * sizeof(lv_color_t);
    };

    static LGFX gfx;
    static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
};
