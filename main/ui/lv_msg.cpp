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

static SemaphoreHandle_t subs_mutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

static inline void lock_subs() { xSemaphoreTake(subs_mutex(), portMAX_DELAY); }
static inline void unlock_subs() { xSemaphoreGive(subs_mutex()); }

lv_msg_sub_dsc_t *lv_msg_subscribe(lv_msg_id_t msg_id, lv_msg_subscribe_cb_t cb, void *user_data) {
  auto *sub = new lv_msg_sub_dsc_t{msg_id, cb, user_data};
  lock_subs();
  s_subs.push_back(sub);
  unlock_subs();
  return sub;
}

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
