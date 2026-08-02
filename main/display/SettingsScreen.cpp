#include "SettingsScreen.h"

#include "LvglWrapper.h"
#include "definitions.h"

#include <esp_log.h>
#include <cstdio>

namespace display {

namespace {
static const char *TAG = "SETTINGS_SCREEN";
constexpr uint16_t kMinMinutes = 1;
constexpr uint16_t kMaxMinutes = 240;
} // namespace

void SettingsScreen::show(lv_obj_t *parent, std::weak_ptr<Screen> parentScreen) {
  (void)parent;
  (void)parentScreen;
  isCleanedUp = false;
  lv_obj_clean(lvObj_);

  settings_ = utilities::loadPowerSettings();
  normalizeCurrentSettings();

  lbl_title = makeLabel(lvObj_, "Settings", LV_ALIGN_TOP_MID, 0, 10, "label.title", &lv_font_montserrat_30);

  lbl_display_off = makeLabel(lvObj_, "Display off (min)", LV_ALIGN_CENTER, -40, -100, "label.main");
  btn_display_off_minus = makeButton(lvObj_, "-", 52, 44, LV_ALIGN_CENTER, -110, -55, "button.secondary");
  btn_display_off_plus = makeButton(lvObj_, "+", 52, 44, LV_ALIGN_CENTER, 110, -55, "button.secondary");
  lbl_display_off_value = makeLabel(lvObj_, "", LV_ALIGN_CENTER, 0, -55, "label.main", &lv_font_montserrat_22);

  lbl_sleep_disc = makeLabel(lvObj_, "Sleep + disconnect (min)", LV_ALIGN_CENTER, -10, 0, "label.main");
  btn_sleep_disc_minus = makeButton(lvObj_, "-", 52, 44, LV_ALIGN_CENTER, -110, 45, "button.secondary");
  btn_sleep_disc_plus = makeButton(lvObj_, "+", 52, 44, LV_ALIGN_CENTER, 110, 45, "button.secondary");
  lbl_sleep_disc_value = makeLabel(lvObj_, "", LV_ALIGN_CENTER, 0, 45, "label.main", &lv_font_montserrat_22);

  lbl_hint = makeLabel(lvObj_, "Sleep/disconnect must be greater than display off.", LV_ALIGN_CENTER, 0, 85, "label.muted");
  lv_obj_set_width(lbl_hint, lv_pct(90));
  lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl_hint, LV_TEXT_ALIGN_CENTER, 0);

  btn_save = makeButton(lvObj_, "Save", 200, 44, LV_ALIGN_BOTTOM_MID, 0, -58, "button.primary");
  btn_back = makeButton(lvObj_, "Back", 200, 44, LV_ALIGN_BOTTOM_MID, 0, -10, "button.secondary");

  lbl_status = makeLabel(lvObj_, "", LV_ALIGN_BOTTOM_MID, 0, -110, "label.main");
  lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_event_cb(btn_display_off_minus, &SettingsScreen::event_display_off_minus_trampoline, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(btn_display_off_plus, &SettingsScreen::event_display_off_plus_trampoline, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(btn_sleep_disc_minus, &SettingsScreen::event_sleep_disc_minus_trampoline, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(btn_sleep_disc_plus, &SettingsScreen::event_sleep_disc_plus_trampoline, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(btn_save, &SettingsScreen::event_save_trampoline, LV_EVENT_CLICKED, this);
  lv_obj_add_event_cb(btn_back, &SettingsScreen::event_back_trampoline, LV_EVENT_CLICKED, this);

  refreshLabels();
  focusedIndex = 0;
  updateFocusedState();
  rotaryAttach();

  ESP_LOGI(TAG, "SettingsScreen UI created");
}

void SettingsScreen::cleanUp() {
  isCleanedUp = true;
  rotaryDetach();

  lbl_title = nullptr;
  lbl_display_off = nullptr;
  lbl_display_off_value = nullptr;
  btn_display_off_minus = nullptr;
  btn_display_off_plus = nullptr;
  lbl_sleep_disc = nullptr;
  lbl_sleep_disc_value = nullptr;
  btn_sleep_disc_minus = nullptr;
  btn_sleep_disc_plus = nullptr;
  lbl_hint = nullptr;
  lbl_status = nullptr;
  btn_save = nullptr;
  btn_back = nullptr;
  focusedIndex = -1;

  lv_obj_clean(lvObj_);
}

void SettingsScreen::normalizeCurrentSettings() {
  if (settings_.displayOffMinutes < kMinMinutes) {
    settings_.displayOffMinutes = kMinMinutes;
  }
  if (settings_.displayOffMinutes > kMaxMinutes) {
    settings_.displayOffMinutes = kMaxMinutes;
  }

  if (settings_.sleepDisconnectMinutes < kMinMinutes) {
    settings_.sleepDisconnectMinutes = kMinMinutes;
  }
  if (settings_.sleepDisconnectMinutes > kMaxMinutes) {
    settings_.sleepDisconnectMinutes = kMaxMinutes;
  }

  if (settings_.sleepDisconnectMinutes <= settings_.displayOffMinutes) {
    if (settings_.displayOffMinutes >= kMaxMinutes) {
      settings_.displayOffMinutes = kMaxMinutes - 1;
    }
    settings_.sleepDisconnectMinutes = settings_.displayOffMinutes + 1;
  }
}

void SettingsScreen::refreshLabels() {
  if (!lbl_display_off_value || !lbl_sleep_disc_value) {
    return;
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(settings_.displayOffMinutes));
  lv_label_set_text(lbl_display_off_value, buf);

  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(settings_.sleepDisconnectMinutes));
  lv_label_set_text(lbl_sleep_disc_value, buf);
}

void SettingsScreen::moveFocus(int direction) {
  if (isCleanedUp || direction == 0) {
    return;
  }

  constexpr int total = 6;
  int idx = focusedIndex;
  if (idx < 0 || idx >= total) {
    idx = 0;
  } else {
    idx = (idx + direction) % total;
    if (idx < 0) {
      idx += total;
    }
  }

  focusedIndex = idx;
  updateFocusedState();
}

void SettingsScreen::updateFocusedState() {
  applyFocusOutline(btn_display_off_minus, focusedIndex == 0);
  applyFocusOutline(btn_display_off_plus, focusedIndex == 1);
  applyFocusOutline(btn_sleep_disc_minus, focusedIndex == 2);
  applyFocusOutline(btn_sleep_disc_plus, focusedIndex == 3);
  applyFocusOutline(btn_save, focusedIndex == 4);
  applyFocusOutline(btn_back, focusedIndex == 5);
}

void SettingsScreen::rotaryMoveFocus(int direction) { moveFocus(direction); }

void SettingsScreen::rotaryActivateFocused() {
  if (isCleanedUp) {
    return;
  }

  if (focusedIndex == 0 && btn_display_off_minus) {
    lv_obj_send_event(btn_display_off_minus, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 1 && btn_display_off_plus) {
    lv_obj_send_event(btn_display_off_plus, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 2 && btn_sleep_disc_minus) {
    lv_obj_send_event(btn_sleep_disc_minus, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 3 && btn_sleep_disc_plus) {
    lv_obj_send_event(btn_sleep_disc_plus, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 4 && btn_save) {
    lv_obj_send_event(btn_save, LV_EVENT_CLICKED, nullptr);
  } else if (focusedIndex == 5 && btn_back) {
    lv_obj_send_event(btn_back, LV_EVENT_CLICKED, nullptr);
  }
}

void SettingsScreen::button_display_off_minus_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  if (settings_.displayOffMinutes > kMinMinutes) {
    settings_.displayOffMinutes--;
  }
  normalizeCurrentSettings();
  refreshLabels();
}

void SettingsScreen::button_display_off_plus_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  if (settings_.displayOffMinutes < kMaxMinutes) {
    settings_.displayOffMinutes++;
  }
  normalizeCurrentSettings();
  refreshLabels();
}

void SettingsScreen::button_sleep_disc_minus_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  if (settings_.sleepDisconnectMinutes > kMinMinutes) {
    settings_.sleepDisconnectMinutes--;
  }
  normalizeCurrentSettings();
  refreshLabels();
}

void SettingsScreen::button_sleep_disc_plus_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  if (settings_.sleepDisconnectMinutes < kMaxMinutes) {
    settings_.sleepDisconnectMinutes++;
  }
  normalizeCurrentSettings();
  refreshLabels();
}

void SettingsScreen::navigateBack() {
  if (auto screen = parentScreen_.lock()) {
    cleanUp();
    screen->showScreen();
  }
}

void SettingsScreen::button_save_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  normalizeCurrentSettings();
  const esp_err_t err = utilities::savePowerSettings(settings_);
  if (err != ESP_OK) {
    if (lbl_status) {
      lv_label_set_text_fmt(lbl_status, "Save failed: %s", esp_err_to_name(err));
      lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  lv_msg_send(MSG_POWER_SETTINGS_UPDATED, nullptr);
  navigateBack();
}

void SettingsScreen::button_back_callback(lv_event_t *e) {
  if (isCleanedUp || lv_event_get_code(e) != LV_EVENT_CLICKED) {
    return;
  }

  navigateBack();
}

} // namespace display
