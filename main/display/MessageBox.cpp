/**
 * @file MessageBox.cpp
 * @brief Modal overlay helper that displays a title, message and OK button.
 *
 * Only one message box may be active at a time. `showMessageBox()` is the
 * single public entry point; any previous overlay is deleted before a new one
 * is created. The optional `onOk` callback is invoked when the user confirms.
 */
#include "MessageBox.h"
#include "LvglWrapper.h"
#include <esp_log.h>

namespace display {

namespace {

static const char *TAG = "MESSAGE_BOX";
static lv_obj_t *s_activeOverlay = nullptr;

struct MessageBoxContext {
  MessageBoxOkCallback onOk = nullptr;
  void *onOkUserData = nullptr;
};

struct StateVisual {
  const char *icon;
  lv_color_t color;
};

StateVisual stateVisual(MessageBoxState state) {
  switch (state) {
  case MessageBoxState::Success:
    return {LV_SYMBOL_OK, lv_palette_main(LV_PALETTE_GREEN)};
  case MessageBoxState::Warning:
    return {LV_SYMBOL_WARNING, lv_palette_main(LV_PALETTE_ORANGE)};
  case MessageBoxState::Error:
    return {LV_SYMBOL_CLOSE, lv_palette_main(LV_PALETTE_RED)};
  case MessageBoxState::Info:
  default:
    return {LV_SYMBOL_BELL, lv_palette_main(LV_PALETTE_BLUE)};
  }
}

// LV_EVENT_DELETE callback: fires when the overlay container is destroyed.
// Clears the global active-overlay pointer and invokes the user callback (if any).
void overlay_delete_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE)
    return;

  auto *ctx = static_cast<MessageBoxContext *>(lv_event_get_user_data(e));
  if (ctx) {
    delete ctx;
  }

  if (s_activeOverlay == lv_event_get_target_obj(e)) {
    s_activeOverlay = nullptr;
  }
}

// Handles the OK button click: deletes the overlay which in turn fires
// overlay_delete_callback and the user-supplied onOk callback.
void ok_button_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;

  auto *ctx = static_cast<MessageBoxContext *>(lv_event_get_user_data(e));
  lv_obj_t *btn = lv_event_get_target_obj(e);
  lv_obj_t *box = lv_obj_get_parent(btn);
  lv_obj_t *overlay = lv_obj_get_parent(box);

  if (ctx != nullptr && ctx->onOk != nullptr) {
    ctx->onOk(ctx->onOkUserData);
  }

  if (overlay != nullptr && lv_obj_is_valid(overlay)) {
    lv_obj_del_async(overlay);
  }
}

} // namespace

// Creates (or replaces) the modal overlay with the supplied title, message,
// visual state style and optional OK callback.
void showMessageBox(const char *title, const char *message, MessageBoxState state, MessageBoxOkCallback onOk,
                    void *onOkUserData) {
  lv_obj_t *screen = lv_screen_active();
  if (screen == nullptr) {
    ESP_LOGW(TAG, "No active screen, cannot show message box");
    return;
  }

  if (s_activeOverlay != nullptr && lv_obj_is_valid(s_activeOverlay)) {
    lv_obj_del(s_activeOverlay);
    s_activeOverlay = nullptr;
  }

  auto *ctx = new MessageBoxContext();
  ctx->onOk = onOk;
  ctx->onOkUserData = onOkUserData;

  lv_obj_t *overlay = lv_obj_create(screen);
  s_activeOverlay = overlay;

  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_event_cb(overlay, overlay_delete_callback, LV_EVENT_DELETE, ctx);

  lv_obj_t *panel = lv_obj_create(overlay);
  lv_obj_set_size(panel, 280, 210);
  lv_obj_center(panel);
  setStyle(panel, "screen.main");
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_border_color(panel, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_obj_set_style_pad_all(panel, 10, 0);

  StateVisual visual = stateVisual(state);
  lv_obj_t *iconLabel = makeLabel(panel, visual.icon, LV_ALIGN_TOP_MID, 0, 8, "label.title", &lv_font_montserrat_30);
  lv_obj_set_style_text_color(iconLabel, visual.color, 0);

  lv_obj_t *titleLabel = makeLabel(panel, title, LV_ALIGN_TOP_MID, 0, 48, "label.title", &lv_font_montserrat_24);
  lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *messageLabel = lv_label_create(panel);
  setStyle(messageLabel, "label.main");
  lv_label_set_text(messageLabel, message);
  lv_label_set_long_mode(messageLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(messageLabel, 250);
  lv_obj_set_style_text_align(messageLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(messageLabel, LV_ALIGN_TOP_MID, 0, 92);

  lv_obj_t *okButton = makeButton(panel, "OK", 90, 40, LV_ALIGN_BOTTOM_MID, 0, -8, "button.primary");
  lv_obj_add_event_cb(okButton, ok_button_callback, LV_EVENT_CLICKED, ctx);
}

} // namespace display
