/**
 * @file lv_msg.cpp
 * @brief Thread-safe lv_msg publish/subscribe implementation.
 *
 * Provides `lv_msg_subscribe`, `lv_msg_unsubscribe` and `lv_msg_send` for the
 * project's inter-component messaging layer. A FreeRTOS mutex guards the
 * subscriber list so that subscriptions can be added/removed from any task
 * while sends are dispatched from the LVGL task.
 */
#include "lv_msg.h"
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

struct lv_msg_sub_dsc_t {
  lv_msg_id_t msg_id;
  lv_msg_subscribe_cb_t cb;
  void *user_data;
};

static std::vector<lv_msg_sub_dsc_t *> s_subs;

// Returns (and lazily creates) the singleton mutex that protects s_subs.
static SemaphoreHandle_t subs_mutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

// Acquires the subscriber list mutex; blocks indefinitely.
static inline void lock_subs() { xSemaphoreTake(subs_mutex(), portMAX_DELAY); }
// Releases the subscriber list mutex.
static inline void unlock_subs() { xSemaphoreGive(subs_mutex()); }

// Registers a callback for the given message ID. Thread-safe. Returns an
// opaque handle that must be passed to lv_msg_unsubscribe to remove the
// subscription.
lv_msg_sub_dsc_t *lv_msg_subscribe(lv_msg_id_t msg_id, lv_msg_subscribe_cb_t cb, void *user_data) {
  auto *sub = new lv_msg_sub_dsc_t{msg_id, cb, user_data};
  lock_subs();
  s_subs.push_back(sub);
  unlock_subs();
  return sub;
}

// Removes a previously registered subscription. Thread-safe. Passing nullptr
// is a no-op.
void lv_msg_unsubscribe(lv_msg_sub_dsc_t *sub) {
  if (!sub)
    return;
  lock_subs();
  auto it = std::find(s_subs.begin(), s_subs.end(), sub);
  if (it != s_subs.end()) {
    s_subs.erase(it);
  }
  unlock_subs();
  delete sub;
}

// Notifies all subscribers whose msg_id matches. Must be called from the LVGL
// task; callbacks fire synchronously before this function returns.
void lv_msg_send(lv_msg_id_t msg_id, const void *payload) {
  /* Snapshot subscribers by value so callbacks can unsubscribe safely while dispatching. */
  std::vector<lv_msg_sub_dsc_t> subs;
  lock_subs();
  subs.reserve(s_subs.size());
  for (auto *s : s_subs) {
    if (s) {
      subs.push_back(*s);
    }
  }
  unlock_subs();

  lv_msg_t msg = {msg_id, payload, nullptr};
  for (const auto &s : subs) {
    if (s.msg_id == msg_id) {
      msg.user_data = s.user_data;
      s.cb(&msg);
    }
  }
}
