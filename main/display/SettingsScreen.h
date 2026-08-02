#pragma once

#include "RotaryListScreenBase.h"
#include "utilities/PowerSettings.h"

namespace display {

class SettingsScreen : public RotaryListScreenBase, public std::enable_shared_from_this<SettingsScreen> {
public:
  static std::shared_ptr<SettingsScreen> instance() {
    static std::shared_ptr<SettingsScreen> s;
    if (!s) {
      s.reset(new SettingsScreen());
    }
    return s;
  }

  ~SettingsScreen() override = default;

  void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
  void cleanUp() override;

  SettingsScreen(const SettingsScreen &) = delete;
  SettingsScreen &operator=(const SettingsScreen &) = delete;

  void button_display_off_minus_callback(lv_event_t *e);
  void button_display_off_plus_callback(lv_event_t *e);
  void button_sleep_disc_minus_callback(lv_event_t *e);
  void button_sleep_disc_plus_callback(lv_event_t *e);
  void button_save_callback(lv_event_t *e);
  void button_back_callback(lv_event_t *e);

protected:
  SettingsScreen() = default;

  static void event_display_off_minus_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_display_off_minus_callback(e);
    }
  }

  static void event_display_off_plus_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_display_off_plus_callback(e);
    }
  }

  static void event_sleep_disc_minus_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_sleep_disc_minus_callback(e);
    }
  }

  static void event_sleep_disc_plus_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_sleep_disc_plus_callback(e);
    }
  }

  static void event_save_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_save_callback(e);
    }
  }

  static void event_back_trampoline(lv_event_t *e) {
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    if (self) {
      self->button_back_callback(e);
    }
  }

private:
  bool rotaryInputEnabled() const override { return !isCleanedUp; }
  void rotaryMoveFocus(int direction) override;
  void rotaryActivateFocused() override;

  void moveFocus(int direction);
  void updateFocusedState();
  void refreshLabels();
  void normalizeCurrentSettings();
  void navigateBack();

  int focusedIndex = -1;
  bool isCleanedUp = false;
  utilities::PowerSettings settings_ = utilities::defaultPowerSettings();

  lv_obj_t *lbl_title = nullptr;
  lv_obj_t *lbl_display_off = nullptr;
  lv_obj_t *lbl_display_off_value = nullptr;
  lv_obj_t *btn_display_off_minus = nullptr;
  lv_obj_t *btn_display_off_plus = nullptr;

  lv_obj_t *lbl_sleep_disc = nullptr;
  lv_obj_t *lbl_sleep_disc_value = nullptr;
  lv_obj_t *btn_sleep_disc_minus = nullptr;
  lv_obj_t *btn_sleep_disc_plus = nullptr;

  lv_obj_t *lbl_hint = nullptr;
  lv_obj_t *lbl_status = nullptr;
  lv_obj_t *btn_save = nullptr;
  lv_obj_t *btn_back = nullptr;
};

} // namespace display
