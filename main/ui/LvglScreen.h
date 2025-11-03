#ifndef _LVGL_SCREEN_H
#define _LVGL_SCREEN_H

#include <lvgl.h>
#include <vector>
#include <memory>
#include <type_traits>
#include "LvglWidgetBase.h"
#include "LvglTheme.h"

namespace ui {

class LvglScreen : public std::enable_shared_from_this<LvglScreen> {
public:
    explicit LvglScreen(lv_obj_t* scr = nullptr)
        : screen_(scr ? scr : lv_obj_create(nullptr))
    {
        lv_screen_load(screen_);   // ✅ replaced lv_scr_load()
        applyThemeToScreen();
    }

    static std::shared_ptr<LvglScreen> createAndLoad() {
        return std::make_shared<LvglScreen>();
    }

    lv_obj_t* lvObj() const { return screen_; }

    // Generic widget factory that tracks instances
    template <typename T, typename... Args>
    T& add(Args&&... args) {
        static_assert(std::is_base_of_v<LvglWidgetBase, T>,
                      "Widget must derive from LvglWidgetBase");
        auto widget = std::make_unique<T>(screen_, std::forward<Args>(args)...);
        T& ref = *widget;
        widgets_.push_back(std::move(widget));
        return ref;
    }

    // Apply active theme to screen + all child widgets
    void applyTheme() {
        applyThemeToScreen();
        for (auto& w : widgets_) {
            w->applyTheme();
        }
    }

    // Optional: call this if theme changes globally
    static void onThemeChanged() {
        for (auto it = activeScreens_.begin(); it != activeScreens_.end();) {
            if (auto s = it->lock()) {
                s->applyTheme();
                ++it;
            } else {
                // remove expired weak_ptrs
                it = activeScreens_.erase(it);
            }
        }
    }

    // Register this screen for automatic re-theming
    void registerScreen() {
        activeScreens_.push_back(shared_from_this());
    }

    // Convenience helpers (new)
    void setBackgroundColor(lv_color_t color) {
        lv_obj_set_style_bg_color(screen_, color, LV_PART_MAIN);
    }

    void setPadding(lv_coord_t pad) {
        lv_obj_set_style_pad_all(screen_, pad, LV_PART_MAIN);
    }

private:
    void applyThemeToScreen() {
        auto theme = LvglTheme::active();
        if (!theme) return;
        lv_obj_set_style_bg_color(screen_, theme->colors().background, LV_PART_MAIN);
    }

    lv_obj_t* screen_ = nullptr;
    std::vector<std::unique_ptr<LvglWidgetBase>> widgets_;
    inline static std::vector<std::weak_ptr<LvglScreen>> activeScreens_;
};

} // namespace ui

#endif
