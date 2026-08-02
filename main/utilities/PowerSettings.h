#pragma once

#include <cstdint>
#include <esp_err.h>

namespace utilities {

struct PowerSettings {
  uint16_t displayOffMinutes;
  uint16_t sleepDisconnectMinutes;
};

constexpr uint16_t kDefaultDisplayOffMinutes = 2;
constexpr uint16_t kDefaultSleepDisconnectMinutes = 5;

PowerSettings defaultPowerSettings();
void normalizePowerSettings(PowerSettings &settings);
PowerSettings loadPowerSettings();
esp_err_t savePowerSettings(const PowerSettings &settings);

} // namespace utilities
