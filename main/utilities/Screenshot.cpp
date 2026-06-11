#include "Screenshot.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "RotaryEncoder.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace utilities {
namespace {

static const char *TAG = "Screenshot";
static bool s_captureInProgress = false;
static int64_t s_lastCaptureUs = 0;
static bool s_inputPausedForCapture = false;

bool takeSnapshotWithExternalBuffer(lv_obj_t *obj, lv_color_format_t cf, lv_draw_buf_t *outBuf, void **outRawBuf,
                                    size_t *outRawSize) {
  if (obj == nullptr || outBuf == nullptr || outRawBuf == nullptr || outRawSize == nullptr) {
    return false;
  }

  lv_obj_update_layout(obj);
  int32_t w = lv_obj_get_width(obj);
  int32_t h = lv_obj_get_height(obj);
  if (w <= 0 || h <= 0) {
    ESP_LOGE(TAG, "Invalid snapshot area: %ldx%ld", static_cast<long>(w), static_cast<long>(h));
    return false;
  }

  const uint32_t stride = lv_draw_buf_width_to_stride(static_cast<uint32_t>(w), cf);
  const size_t rawSize = LV_DRAW_BUF_SIZE(static_cast<uint32_t>(w), static_cast<uint32_t>(h), cf);

  void *raw = heap_caps_malloc(rawSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (raw == nullptr) {
    ESP_LOGW(TAG, "PSRAM alloc failed for snapshot (%u bytes), trying internal RAM", static_cast<unsigned>(rawSize));
    raw = heap_caps_malloc(rawSize, MALLOC_CAP_8BIT);
  }
  if (raw == nullptr) {
    ESP_LOGE(TAG, "Snapshot buffer alloc failed (%u bytes)", static_cast<unsigned>(rawSize));
    return false;
  }

  if (lv_draw_buf_init(outBuf, static_cast<uint32_t>(w), static_cast<uint32_t>(h), cf, stride, raw,
                       static_cast<uint32_t>(rawSize)) != LV_RESULT_OK) {
    ESP_LOGE(TAG, "lv_draw_buf_init failed");
    free(raw);
    return false;
  }

  if (lv_snapshot_take_to_draw_buf(obj, cf, outBuf) != LV_RESULT_OK) {
    ESP_LOGE(TAG, "lv_snapshot_take_to_draw_buf failed");
    free(raw);
    return false;
  }

  *outRawBuf = raw;
  *outRawSize = rawSize;
  return true;
}

bool ensureScreenshotStorageMountedImpl() {
  static bool initialized = false;
  static bool mounted = false;

  if (initialized) {
    return mounted;
  }

  initialized = true;

  esp_vfs_spiffs_conf_t conf = {};
  conf.base_path = "/spiffs";
  conf.partition_label = "spiffs";
  conf.max_files = 4;
  conf.format_if_mount_failed = true;

  const esp_err_t regErr = esp_vfs_spiffs_register(&conf);
  if (regErr != ESP_OK && regErr != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(regErr));
    mounted = false;
    return false;
  }

  size_t total = 0;
  size_t used = 0;
  const esp_err_t infoErr = esp_spiffs_info(conf.partition_label, &total, &used);
  if (infoErr == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS mounted at /spiffs (used=%u total=%u)", static_cast<unsigned>(used),
             static_cast<unsigned>(total));
  }

  mounted = true;
  return true;
}

} // namespace

bool ensureScreenshotStorageMounted() { return ensureScreenshotStorageMountedImpl(); }

CaptureStatus getCaptureStatus() { return s_captureInProgress ? CaptureStatus::InProgress : CaptureStatus::Ready; }

bool saveActiveScreenScreenshot() {
#if LV_USE_SNAPSHOT
  constexpr int64_t SCREENSHOT_COOLDOWN_US = 1000000;

  if (s_captureInProgress) {
    ESP_LOGW(TAG, "Screenshot request ignored: capture already in progress");
    return false;
  }

  const int64_t nowUs = esp_timer_get_time();
  if ((nowUs - s_lastCaptureUs) < SCREENSHOT_COOLDOWN_US) {
    ESP_LOGW(TAG, "Screenshot request ignored: cooldown active");
    return false;
  }

  s_captureInProgress = true;

  if (!ensureScreenshotStorageMounted()) {
    s_captureInProgress = false;
    return false;
  }

  pauseInputDuringCapture();
  ESP_LOGI(TAG, "Taking screenshot...");

  lv_display_t *disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }

  lv_obj_t *screen = lv_screen_active();
  if (screen == nullptr) {
    ESP_LOGE(TAG, "No active LVGL screen");
    resumeInputAfterCapture();
    s_captureInProgress = false;
    return false;
  }

  lv_draw_buf_t snapshot = {};
  void *snapshotRaw = nullptr;
  size_t snapshotRawSize = 0;

  if (!takeSnapshotWithExternalBuffer(screen, LV_COLOR_FORMAT_RGB565, &snapshot, &snapshotRaw, &snapshotRawSize)) {
    resumeInputAfterCapture();
    s_captureInProgress = false;
    return false;
  }

  const uint32_t width = snapshot.header.w;
  const uint32_t height = snapshot.header.h;
  const uint32_t stride = snapshot.header.stride;

  // Check available space before attempting write
  size_t spiffsUsed = 0;
  size_t spiffsTotal = 0;
  if (esp_spiffs_info("spiffs", &spiffsTotal, &spiffsUsed) == ESP_OK) {
    const size_t spiffsFree = spiffsTotal - spiffsUsed;
    const size_t requiredSpace = (width * height * 3) + 512; // PPM header + pixel data
    ESP_LOGI(TAG, "SPIFFS: %u bytes free, %u bytes required", static_cast<unsigned>(spiffsFree),
             static_cast<unsigned>(requiredSpace));
    if (spiffsFree < requiredSpace) {
      ESP_LOGE(TAG, "Insufficient SPIFFS space: need %u, have %u", static_cast<unsigned>(requiredSpace),
               static_cast<unsigned>(spiffsFree));
      free(snapshotRaw);
      resumeInputAfterCapture();
      s_captureInProgress = false;
      return false;
    }
  }

  const char *tmpPath = "/spiffs/screenshot_latest.tmp";
  const char *finalPath = "/spiffs/screenshot_latest.ppm";

  FILE *f = std::fopen(tmpPath, "wb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open screenshot file: %s", tmpPath);
    free(snapshotRaw);
    resumeInputAfterCapture();
    s_captureInProgress = false;
    return false;
  }

  std::fprintf(f, "P6\n%u %u\n255\n", static_cast<unsigned>(width), static_cast<unsigned>(height));

  std::vector<uint8_t> line;
  line.resize(width * 3U);

  for (uint32_t y = 0; y < height; ++y) {
    const uint8_t *row = snapshot.data + (y * stride);
    for (uint32_t x = 0; x < width; ++x) {
      const uint16_t px = (static_cast<uint16_t>(row[x * 2U + 1U]) << 8U) | row[x * 2U];
      const uint8_t r5 = static_cast<uint8_t>((px >> 11U) & 0x1FU);
      const uint8_t g6 = static_cast<uint8_t>((px >> 5U) & 0x3FU);
      const uint8_t b5 = static_cast<uint8_t>(px & 0x1FU);

      line[x * 3U + 0U] = static_cast<uint8_t>((r5 * 255U) / 31U);
      line[x * 3U + 1U] = static_cast<uint8_t>((g6 * 255U) / 63U);
      line[x * 3U + 2U] = static_cast<uint8_t>((b5 * 255U) / 31U);
    }

    if (std::fwrite(line.data(), 1, line.size(), f) != line.size()) {
      ESP_LOGE(TAG, "Failed writing screenshot data");
      std::fclose(f);
      free(snapshotRaw);
      resumeInputAfterCapture();
      s_captureInProgress = false;
      return false;
    }
  }

  std::fclose(f);

  // Replace existing file atomically-ish: remove old, then rename new temp file.
  std::remove(finalPath);
  if (std::rename(tmpPath, finalPath) != 0) {
    ESP_LOGE(TAG, "Failed to replace screenshot file: %s", finalPath);
    std::remove(tmpPath);
    free(snapshotRaw);
    resumeInputAfterCapture();
    s_captureInProgress = false;
    return false;
  }

  free(snapshotRaw);
  s_lastCaptureUs = esp_timer_get_time();
  s_captureInProgress = false;
  resumeInputAfterCapture();

  ESP_LOGI(TAG, "Saved screenshot to %s", finalPath);
  return true;
#else
  ESP_LOGW(TAG, "LV_USE_SNAPSHOT is disabled");
  return false;
#endif
}

bool isInputPausedForCapture() { return s_inputPausedForCapture; }

void pauseInputDuringCapture() {
  if (s_inputPausedForCapture) {
    return;
  }
  s_inputPausedForCapture = true;
  RotaryEncoder::instance()->pause();
  ESP_LOGI(TAG, "Input paused for screenshot capture");
}

void resumeInputAfterCapture() {
  if (!s_inputPausedForCapture) {
    return;
  }
  s_inputPausedForCapture = false;
  RotaryEncoder::instance()->resume();
  ESP_LOGI(TAG, "Input resumed after screenshot capture");
}

} // namespace utilities
