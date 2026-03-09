#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_TURNOUTCLOSED
#define LV_ATTRIBUTE_IMG_TURNOUTCLOSED
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_TURNOUTCLOSED uint8_t turnoutclosed_map[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x7f, 0x00, 0x01, 0xc0, 0x00, 0xe0,
    0x00, 0x70, 0x7f, 0xfe, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const lv_image_dsc_t turnoutclosed = {
    .header =
        {
            .cf = LV_COLOR_FORMAT_A1,
            .w = 16,
            .h = 16,
            .stride = 2,
        },
    .data_size = 32,
    .data = turnoutclosed_map,
};
