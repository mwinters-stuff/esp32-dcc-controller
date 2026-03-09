#include "lv_msg.h"
#include <algorithm>
#include <vector>

struct lv_msg_sub_dsc_t {
  lv_msg_id_t msg_id;
  lv_msg_subscribe_cb_t cb;
  void *user_data;
};

static std::vector<lv_msg_sub_dsc_t *> s_subs;

lv_msg_sub_dsc_t *lv_msg_subscribe(lv_msg_id_t msg_id, lv_msg_subscribe_cb_t cb, void *user_data) {
  auto *sub = new lv_msg_sub_dsc_t{msg_id, cb, user_data};
  s_subs.push_back(sub);
  return sub;
}

void lv_msg_unsubscribe(lv_msg_sub_dsc_t *sub) {
  if (!sub)
    return;
  auto it = std::find(s_subs.begin(), s_subs.end(), sub);
  if (it != s_subs.end()) {
    s_subs.erase(it);
  }
  delete sub;
}

void lv_msg_send(lv_msg_id_t msg_id, const void *payload) {
  /* Iterate a snapshot so callbacks can safely unsubscribe during delivery */
  auto subs = s_subs;
  lv_msg_t msg = {msg_id, payload, nullptr};
  for (auto *s : subs) {
    if (s && s->msg_id == msg_id) {
      msg.user_data = s->user_data;
      s->cb(&msg);
    }
  }
}
