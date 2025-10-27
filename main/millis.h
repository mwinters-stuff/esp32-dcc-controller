#ifndef _MILLIS_H
#define _MILLIS_H

#include <esp_timer.h>

inline uint64_t millis() {
  return esp_timer_get_time() / 1000ULL;
}

#endif