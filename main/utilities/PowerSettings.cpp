#include "PowerSettings.h"
#include "definitions.h"

#include <esp_log.h>
#include <nvs_handle.hpp>

namespace utilities {

namespace {
static const char *TAG = "PowerSettings";
constexpr uint16_t kMinMinutes = 1;
constexpr uint16_t kMaxMinutes = 240;
} // namespace

PowerSettings defaultPowerSettings() { return PowerSettings{kDefaultDisplayOffMinutes, kDefaultSleepDisconnectMinutes}; }

void normalizePowerSettings(PowerSettings &settings) {
  if (settings.displayOffMinutes < kMinMinutes) {
    settings.displayOffMinutes = kMinMinutes;
  }
  if (settings.displayOffMinutes > kMaxMinutes) {
    settings.displayOffMinutes = kMaxMinutes;
  }

  if (settings.sleepDisconnectMinutes < kMinMinutes) {
    settings.sleepDisconnectMinutes = kMinMinutes;
  }
  if (settings.sleepDisconnectMinutes > kMaxMinutes) {
    settings.sleepDisconnectMinutes = kMaxMinutes;
  }

  if (settings.sleepDisconnectMinutes <= settings.displayOffMinutes) {
    if (settings.displayOffMinutes >= kMaxMinutes) {
      settings.displayOffMinutes = kMaxMinutes - 1;
    }
    settings.sleepDisconnectMinutes = settings.displayOffMinutes + 1;
  }
}

PowerSettings loadPowerSettings() {
  PowerSettings settings = defaultPowerSettings();

  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_SETTINGS, NVS_READONLY, &err);
  if (err != ESP_OK || !handle) {
    normalizePowerSettings(settings);
    return settings;
  }

  uint16_t displayOff = settings.displayOffMinutes;
  if (handle->get_item(NVS_DISPLAY_OFF_MINUTES, displayOff) == ESP_OK) {
    settings.displayOffMinutes = displayOff;
  }

  uint16_t sleepDisconnect = settings.sleepDisconnectMinutes;
  if (handle->get_item(NVS_SLEEP_DISCONNECT_MINUTES, sleepDisconnect) == ESP_OK) {
    settings.sleepDisconnectMinutes = sleepDisconnect;
  }

  normalizePowerSettings(settings);
  return settings;
}

esp_err_t savePowerSettings(const PowerSettings &input) {
  PowerSettings settings = input;
  normalizePowerSettings(settings);

  esp_err_t err = ESP_OK;
  std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_SETTINGS, NVS_READWRITE, &err);
  if (err != ESP_OK || !handle) {
    ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return err;
  }

  err = handle->set_item(NVS_DISPLAY_OFF_MINUTES, settings.displayOffMinutes);
  if (err != ESP_OK) {
    return err;
  }

  err = handle->set_item(NVS_SLEEP_DISCONNECT_MINUTES, settings.sleepDisconnectMinutes);
  if (err != ESP_OK) {
    return err;
  }

  err = handle->commit();
  if (err != ESP_OK) {
    return err;
  }

  return ESP_OK;
}

} // namespace utilities
