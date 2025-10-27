#include "lvgl.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "display_manager.h"
#include <array>
#include <cmath>

namespace display {

static const char* TAG = "ManualCal";

static lv_obj_t* lbl_status = nullptr;
static int calibration_index = 0;
static std::array<lv_point_t, 5> raw_points;
static std::array<lv_point_t, 5> ref_points;

// calibration parameters
static float scale_x = 1.0f, scale_y = 1.0f;
static float offset_x = 0.0f, offset_y = 0.0f;
#define STORAGE_MULTIPLIER 100.0f

// --- forward declarations ---
static void compute_calibration();
static void show_target(lv_obj_t* parent, int idx);
static void save_to_nvs();
static bool load_from_nvs();

// ================================================================
// Public API
// ================================================================

bool read_raw_touch(int16_t* x, int16_t* y)
{
    uint16_t tx, ty;
    bool pressed = DisplayManager::gfx.getTouch(&tx, &ty);

    if (pressed) {
        *x = static_cast<int16_t>(tx);
        *y = static_cast<int16_t>(ty);
    }

    return pressed;
}

bool loadCalibrationFromNVS()
{
    return load_from_nvs();
}

void applyTouchTransform(int16_t* x, int16_t* y)
{
    // Apply calibration scaling
    *x = static_cast<int16_t>(*x * scale_x + offset_x);
    *y = static_cast<int16_t>(*y * scale_y + offset_y);
}

void startManualCalibration(lv_obj_t* parent)
{
    calibration_index = 0;

    // Tell LVGL flush to use non-DMA path while we calibrate
    DisplayManager::calibrating.store(true, std::memory_order_release);

    // Drain any in-flight DMA and ensure we own the panel (best-effort)
    if (DisplayManager::gfx.getStartCount() > 0) {
        DisplayManager::gfx.endWrite();
    }
    // waitDMA can crash on some setups; if it worked for you earlier you can keep it,
    // otherwise comment it out. Try with it first:
    DisplayManager::gfx.waitDMA();

    // Now draw the first target via LVGL safely (flush will use blocking path)
    lv_obj_clean(parent);
    lbl_status = nullptr;
    show_target(parent, 0);

    // Create timer that polls for a touch input
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
        lv_obj_t* parent = (lv_obj_t*)t->user_data;
         static bool waiting_release = false;

        int16_t x, y;

         if (!read_raw_touch(&x, &y)) {
            // Not pressed
            waiting_release = false;
            return;
        }

        // If we’re waiting for release, ignore continued press
        if (waiting_release) return;
        waiting_release = true;

        ESP_LOGI(TAG, "Got touch %d: %d,%d", calibration_index, x, y);
        raw_points[calibration_index] = { x, y };
        calibration_index++;

        if (calibration_index < 5) {
            show_target(parent, calibration_index);
        } else {
            lv_timer_del(t);
            compute_calibration();

            // restore normal LVGL/DMA behavior
            DisplayManager::calibrating.store(false, std::memory_order_release);

            // Force LVGL to redraw now the flag is cleared
            lv_timer_handler();
            lv_refr_now(lv_disp_get_default());
        }
    }, 100, parent);

    lv_timer_set_repeat_count(timer, -1);
    ESP_LOGI(TAG, "Manual 5-point calibration started");
}


// ================================================================
// Internal helpers
// ================================================================

static void show_target(lv_obj_t* parent, int idx)
{
    lv_obj_clean(parent);

    lv_coord_t w = lv_obj_get_width(parent);
    lv_coord_t h = lv_obj_get_height(parent);
    lv_coord_t margin = 40;

    lv_point_t pt;
    switch (idx) {
        case 0: pt = { margin, margin }; break;                   // top-left
        case 1: pt = { lv_coord_t(w - margin), margin }; break;               // top-right
        case 2: pt = { lv_coord_t(w - margin), lv_coord_t(h - margin) }; break;           // bottom-right
        case 3: pt = { margin, lv_coord_t(h - margin) }; break;               // bottom-left
        case 4: pt = { lv_coord_t(w / 2), lv_coord_t(h / 2) }; break;                     // center
        default: pt = { lv_coord_t(w / 2), lv_coord_t(h / 2) }; break;
    }

    ref_points[idx] = pt;

    lv_obj_t* cross = lv_obj_create(parent);
    lv_obj_set_size(cross, 20, 20);
    lv_obj_set_style_bg_color(cross, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(cross, LV_ALIGN_TOP_LEFT, pt.x - 10, pt.y - 10);

    lbl_status = lv_label_create(parent);
    lv_label_set_text_fmt(lbl_status, "Touch point %d of 5", idx + 1);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -20);
}

static void compute_calibration()
{
    // Simple linear model based on 4 corners (center used for fine tuning)
    int x_min = raw_points[0].x;
    int y_min = raw_points[0].y;
    int x_max = raw_points[2].x;
    int y_max = raw_points[2].y;

    scale_x = float(ref_points[2].x - ref_points[0].x) / (x_max - x_min);
    scale_y = float(ref_points[2].y - ref_points[0].y) / (y_max - y_min);
    offset_x = ref_points[0].x - x_min * scale_x;
    offset_y = ref_points[0].y - y_min * scale_y;

    // Optional fine-tune: adjust offset using center point
    {
        float cx_measured = raw_points[4].x * scale_x + offset_x;
        float cy_measured = raw_points[4].y * scale_y + offset_y;
        float dx = ref_points[4].x - cx_measured;
        float dy = ref_points[4].y - cy_measured;
        offset_x += dx * 0.5f;
        offset_y += dy * 0.5f;
    }

    ESP_LOGI(TAG, "Calibration done:");
    ESP_LOGI(TAG, "scale_x=%.3f scale_y=%.3f offset_x=%.1f offset_y=%.1f",
             scale_x, scale_y, offset_x, offset_y);

    save_to_nvs();

    lv_label_set_text(lbl_status, "Calibration complete!");
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 0);
}

static void save_to_nvs()
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("touch_cal", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    nvs_set_u32(nvs, "scale_x", scale_x * STORAGE_MULTIPLIER);
    nvs_set_u32(nvs, "scale_y", scale_y * STORAGE_MULTIPLIER);
    nvs_set_u32(nvs, "offset_x", offset_x * STORAGE_MULTIPLIER);
    nvs_set_u32(nvs, "offset_y", offset_y * STORAGE_MULTIPLIER);
    nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Saved calibration to NVS");
}

static bool load_from_nvs()
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("touch_cal", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing calibration data");
        return false;
    }

    uint32_t sx,sy,ox,oy;
    nvs_get_u32(nvs, "scale_x", &sx);
    nvs_get_u32(nvs, "scale_y", &sy);
    nvs_get_u32(nvs, "offset_x", &ox);
    nvs_get_u32(nvs, "offset_y", &oy);
    nvs_close(nvs);

    scale_x = sx / STORAGE_MULTIPLIER;
    scale_y = sy / STORAGE_MULTIPLIER;
    offset_x = ox / STORAGE_MULTIPLIER;
    offset_y = oy / STORAGE_MULTIPLIER;

    ESP_LOGI(TAG, "Loaded calibration from NVS: "
                  "scale_x=%.3f scale_y=%.3f offset_x=%.1f offset_y=%.1f",
             scale_x, scale_y, offset_x, offset_y);
    return true;
}

} // namespace display
