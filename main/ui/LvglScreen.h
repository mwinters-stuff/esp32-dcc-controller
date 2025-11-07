#ifndef _LVGL_SCREEN_H
#define _LVGL_SCREEN_H

#include <lvgl.h>
#include <vector>
#include <memory>
#include <type_traits>
#include "LvglWidgetBase.h"
#include "LvglTheme.h"

namespace ui {

class LvglScreen : public std::enable_shared_from_this<LvglScreen>{
public:
    explicit LvglScreen(lv_obj_t* scr = nullptr)
        : screen_(scr ? scr : lv_obj_create(nullptr))
    {
        lv_scr_load(screen_);
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


/*
Update Theme Switching

Now, you can swap themes dynamically and automatically restyle all widgets:

#include "LvglScreen.h"
#include "LvglButton.h"
#include "LvglLabel.h"
#include "LvglTheme.h"

using namespace ui;

void create_ui() {
    auto light = makeLightTheme();
    auto dark  = makeDarkTheme();

    // Start with light theme
    LvglTheme::setActive(light);

    auto screen = LvglScreen::createAndLoad();
    screen->registerScreen();  // enable auto re-theming

    auto& label = screen->add<LvglLabel>("Hello Theme World!");
    auto& button = screen->add<LvglButton>("Toggle Theme", [](lv_event_t* e) {
        // Switch theme on button press
        static bool darkMode = false;
        darkMode = !darkMode;
        LvglTheme::setActive(darkMode ? makeDarkTheme() : makeLightTheme());
        LvglScreen::onThemeChanged(); // auto-update all widgets
    });
}
    


#include "LvglScreen.h"
#include "LvglLabel.h"
#include "LvglButton.h"

using namespace ui;

static std::unique_ptr<LvglScreen> mainScreen;
static std::unique_ptr<LvglScreen> settingsScreen;

void showSettingsScreen();

void showMainScreen() {
    mainScreen = LvglScreen::createAndLoad();

    mainScreen->setBackgroundColor(lv_palette_main(LV_PALETTE_BLUE));
    mainScreen->setPadding(10);

    auto& lbl = mainScreen->add<LvglLabel>("Main Menu");
    lbl.setFont(&lv_font_montserrat_22);
    lbl.setColor(lv_color_white());

    auto& btn = mainScreen->add<LvglButton>("Settings", [](lv_event_t* e) {
        showSettingsScreen();
    });
    btn.setFont(&lv_font_montserrat_18);
}

void showSettingsScreen() {
    settingsScreen = LvglScreen::createAndLoad(true);

    settingsScreen->setBackgroundColor(lv_palette_lighten(LV_PALETTE_GREY, 2));
    settingsScreen->setPadding(8);

    auto& lbl = settingsScreen->add<LvglLabel>("Settings");
    lbl.setFont(&lv_font_montserrat_20);

    auto& btn = settingsScreen->add<LvglButton>("Back", [](lv_event_t* e) {
        showMainScreen();
    });
    btn.setFont(&lv_font_montserrat_18);
}

// Call this once after LVGL init
void ui_init() {
    showMainScreen();
}

*/