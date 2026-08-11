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
static constexpr lv_coord_t kPanelWidth = 280;
static constexpr lv_coord_t kPanelMinHeight = 210;
static constexpr lv_coord_t kMessageWidth = 250;
static constexpr lv_coord_t kButtonWidth = 90;
static constexpr lv_coord_t kButtonHeight = 40;
static constexpr lv_coord_t kPanelPad = 10;
static constexpr lv_coord_t kPanelGap = 12;
static constexpr lv_coord_t kButtonRowGap = 12;

struct MessageBoxContext {
  MessageBoxOkCallback onOk = nullptr;
  void *onOkUserData = nullptr;
  MessageBoxOkCallback onCancel = nullptr;
  void *onCancelUserData = nullptr;
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

lv_obj_t *overlay_from_button(lv_obj_t *btn) {
  lv_obj_t *buttonRow = btn ? lv_obj_get_parent(btn) : nullptr;
  lv_obj_t *panel = buttonRow ? lv_obj_get_parent(buttonRow) : nullptr;
  return panel ? lv_obj_get_parent(panel) : nullptr;
}

void ok_button_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;

  auto *ctx = static_cast<MessageBoxContext *>(lv_event_get_user_data(e));
  lv_obj_t *btn = lv_event_get_target_obj(e);
  lv_obj_t *overlay = overlay_from_button(btn);

  if (ctx != nullptr && ctx->onOk != nullptr) {
    ctx->onOk(ctx->onOkUserData);
  }

  if (overlay != nullptr && lv_obj_is_valid(overlay)) {
    lv_obj_del_async(overlay);
  }
}

void cancel_button_callback(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    return;

  auto *ctx = static_cast<MessageBoxContext *>(lv_event_get_user_data(e));
  lv_obj_t *btn = lv_event_get_target_obj(e);
  lv_obj_t *overlay = overlay_from_button(btn);

  if (ctx != nullptr && ctx->onCancel != nullptr) {
    ctx->onCancel(ctx->onCancelUserData);
  }

  if (overlay != nullptr && lv_obj_is_valid(overlay)) {
    lv_obj_del_async(overlay);
  }
}

void buildMessageBox(const char *title, const char *message, MessageBoxState state, MessageBoxOkCallback onOk,
                     void *onOkUserData, MessageBoxOkCallback onCancel, void *onCancelUserData, bool showCancelButton) {
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
  ctx->onCancel = onCancel;
  ctx->onCancelUserData = onCancelUserData;

  lv_obj_t *overlay = lv_obj_create(screen);
  s_activeOverlay = overlay;

  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_event_cb(overlay, overlay_delete_callback, LV_EVENT_DELETE, ctx);

  lv_obj_t *panel = lv_obj_create(overlay);
  lv_obj_set_size(panel, kPanelWidth, LV_SIZE_CONTENT);
  lv_obj_center(panel);
  setStyle(panel, "screen.main");
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_border_color(panel, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_obj_set_style_pad_all(panel, kPanelPad, 0);
  lv_obj_set_style_pad_gap(panel, kPanelGap, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_min_height(panel, kPanelMinHeight, 0);

  StateVisual visual = stateVisual(state);
  lv_obj_t *iconLabel = lv_label_create(panel);
  lv_label_set_text(iconLabel, visual.icon);
  setStyle(iconLabel, "label.title");
  lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_30, 0);
  lv_obj_set_style_text_color(iconLabel, visual.color, 0);
  lv_obj_set_style_text_align(iconLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(iconLabel, LV_PCT(100));

  lv_obj_t *titleLabel = lv_label_create(panel);
  lv_label_set_text(titleLabel, title);
  setStyle(titleLabel, "label.title");
  lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(titleLabel, LV_PCT(100));

  lv_obj_t *messageLabel = lv_label_create(panel);
  setStyle(messageLabel, "label.main");
  lv_label_set_text(messageLabel, message);
  lv_label_set_long_mode(messageLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(messageLabel, kMessageWidth);
  lv_obj_set_style_text_align(messageLabel, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *buttonRow = lv_obj_create(panel);
  lv_obj_set_size(buttonRow, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(buttonRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(buttonRow, 0, 0);
  lv_obj_set_style_outline_width(buttonRow, 0, 0);
  lv_obj_set_style_pad_all(buttonRow, 0, 0);
  lv_obj_set_style_pad_gap(buttonRow, kButtonRowGap, 0);
  lv_obj_clear_flag(buttonRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(buttonRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttonRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  if (showCancelButton) {
    lv_obj_t *cancelButton =
        makeButton(buttonRow, "Cancel", kButtonWidth, kButtonHeight, LV_ALIGN_DEFAULT, 0, 0, "button.secondary");
    lv_obj_add_event_cb(cancelButton, cancel_button_callback, LV_EVENT_CLICKED, ctx);
  }

  lv_obj_t *okButton =
      makeButton(buttonRow, "OK", kButtonWidth, kButtonHeight, LV_ALIGN_DEFAULT, 0, 0, "button.primary");
  lv_obj_add_event_cb(okButton, ok_button_callback, LV_EVENT_CLICKED, ctx);

  lv_obj_update_layout(panel);
  lv_obj_center(panel);
}

} // namespace

// Creates (or replaces) the modal overlay with the supplied title, message,
// visual state style and optional OK callback.
void showMessageBox(const char *title, const char *message, MessageBoxState state, MessageBoxOkCallback onOk,
                    void *onOkUserData) {
  buildMessageBox(title, message, state, onOk, onOkUserData, nullptr, nullptr, false);
}

void showConfirmMessageBox(const char *title, const char *message, MessageBoxState state,
                           MessageBoxOkCallback onConfirm, void *onConfirmUserData, MessageBoxOkCallback onCancel,
                           void *onCancelUserData) {
  buildMessageBox(title, message, state, onConfirm, onConfirmUserData, onCancel, onCancelUserData, true);
}

} // namespace display
