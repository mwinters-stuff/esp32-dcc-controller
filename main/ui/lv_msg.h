/**
 * Minimal lv_msg shim for projects migrated from LVGL 8 to LVGL 9.
 * lv_msg was removed from LVGL 9; this file re-implements the same API.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t lv_msg_id_t;

typedef struct {
  lv_msg_id_t id;
  const void *payload;
  void *user_data;
} lv_msg_t;

typedef struct lv_msg_sub_dsc_t lv_msg_sub_dsc_t;

typedef void (*lv_msg_subscribe_cb_t)(lv_msg_t *msg);

lv_msg_sub_dsc_t *lv_msg_subscribe(lv_msg_id_t msg_id, lv_msg_subscribe_cb_t cb, void *user_data);
void lv_msg_unsubscribe(lv_msg_sub_dsc_t *sub);
void lv_msg_send(lv_msg_id_t msg_id, const void *payload);

static inline void *lv_msg_get_user_data(lv_msg_t *msg) { return msg ? msg->user_data : 0; }
static inline const void *lv_msg_get_payload(lv_msg_t *msg) { return msg ? msg->payload : 0; }

#ifdef __cplusplus
}
#endif
