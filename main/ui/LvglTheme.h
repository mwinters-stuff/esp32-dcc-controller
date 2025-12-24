#pragma once

#include "LvglStyle.h"
#include <lvgl.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace ui
{

  class LvglTheme
  {
  public:
    struct Palette
    {
      lv_color_t primary = lv_palette_main(LV_PALETTE_BLUE);
      lv_color_t secondary = lv_palette_main(LV_PALETTE_CYAN);
      lv_color_t background = lv_color_white();
      lv_color_t surface = lv_palette_lighten(LV_PALETTE_GREY, 3);
      lv_color_t text = lv_color_black();
      lv_color_t border = lv_palette_darken(LV_PALETTE_GREY, 1);
      lv_color_t success = lv_palette_main(LV_PALETTE_GREEN);
      lv_color_t error = lv_palette_main(LV_PALETTE_RED);
      lv_color_t white = lv_color_white();
      lv_color_t black = lv_color_black();
    };

    struct Metrics
    {
      lv_coord_t radius = 6;
      lv_coord_t padding = 8;
      lv_coord_t spacing = 6;
    };

    struct Fonts
    {
      const lv_font_t *small = &lv_font_montserrat_14;
      const lv_font_t *medium = &lv_font_montserrat_16;
      const lv_font_t *large = &lv_font_montserrat_24;
      const lv_font_t *xlarge = &lv_font_montserrat_30;
    };

    LvglTheme(std::string name,
              const Palette &palette,
              const Metrics &metrics,
              const Fonts &fonts)
        : name_(std::move(name)),
          palette_(palette),
          metrics_(metrics),
          fonts_(fonts)
    {
      buildDefaultStyles();
    }

    // optional convenience constructor with defaults
    explicit LvglTheme(std::string name)
        : name_(std::move(name))
    {
      buildDefaultStyles();
    }

    // Accessors
    const std::string &name() const { return name_; }
    const Palette &colors() const { return palette_; }
    const Metrics &metrics() const { return metrics_; }
    const Fonts &fonts() const { return fonts_; }

    // Define or get a style (creates if missing)
    LvglStyle &defineStyle(const std::string &key)
    {
      auto it = styles_.find(key);
      if (it == styles_.end())
      {
        auto up = std::make_unique<LvglStyle>();
        LvglStyle &ref = *up;
        styles_.emplace(key, std::move(up));
        return ref;
      }
      return *it->second;
    }

    // Get pointer or reference
    const LvglStyle *get(const std::string &key) const
    {
      auto it = styles_.find(key);
      return (it != styles_.end()) ? it->second.get() : nullptr;
    }

    //     std::shared_ptr<LvglStyle> get(const std::string& name) {
    //     auto it = styles_.find(name);
    //     if (it != styles_.end())
    //         return it->secondt;
    //     return nullptr;
    // }

    const LvglStyle &style(const std::string &key) const
    {
      if (auto p = get(key))
        return *p;
      auto dot = key.find('.');
      if (dot != std::string::npos)
      {
        auto base = key.substr(0, dot);
        if (auto p2 = get(base))
          return *p2;
      }
      static LvglStyle empty;
      return empty;
    }

    // Global theme
    static void setActive(std::shared_ptr<LvglTheme> theme) { active_ = std::move(theme); }
    static std::shared_ptr<LvglTheme> active() { return active_; }

  private:
    // Register default per-widget styles
    void buildDefaultStyles()
    {
      // ---- BUTTON ----
      defineStyle("button.main")
          .bgColor(palette_.primary)
          .textColor(lv_color_white())
          .radius(metrics_.radius)
          .padAll(metrics_.padding);

      defineStyle("button.pressed")
          .bgColor(lv_palette_darken(LV_PALETTE_BLUE, 2))
          .textColor(lv_color_white());

      defineStyle("button.disabled")
          .bgColor(palette_.surface)
          .textColor(lv_palette_lighten(LV_PALETTE_GREY, 2));
      defineStyle("button.secondary")
          .bgColor(palette_.secondary)
          .textColor(lv_color_white());

      defineStyle("button.primary")
          .bgColor(palette_.primary)
          .textColor(lv_color_white());

      // ---- LABEL ----
      defineStyle("label.main")
          .textColor(palette_.text)
          .textFont(fonts_.medium);

      defineStyle("label.title")
          .textColor(palette_.primary)
          .textFont(fonts_.xlarge);

      defineStyle("label.muted")
          .textColor(palette_.text)
          .textFont(fonts_.small);


      // ---- CHECKBOX ----
      defineStyle("checkbox.main")
          .textColor(palette_.text)
          .padAll(metrics_.padding / 2);

      defineStyle("checkbox.checked")
          .bgColor(palette_.primary);

      defineStyle("checkbox.disabled")
          .textColor(lv_palette_lighten(LV_PALETTE_GREY, 2));

      // ---- DROPDOWN ----
      defineStyle("dropdown.main")
          .bgColor(palette_.surface)
          .textColor(palette_.text)
          .radius(metrics_.radius)
          .padAll(metrics_.padding);

      defineStyle("dropdown.selected")
          .bgColor(palette_.primary)
          .textColor(lv_color_white());

      defineStyle("screen.main")
          .bgColor(palette_.background)
          .textColor(palette_.text);

      defineStyle("list.main")
          .bgColor(palette_.background)
          .borderWidth(2)
          .padAll(6);

      defineStyle("wifi.item")
          .bgColor(palette_.background);
          // .borderColor(lv_color_hex(0xD0D0D0));
          // .borderWidth(1)
          // .padAll(6);
      defineStyle("wifi.item.selected")
          .bgColor(lv_color_hex(0x3399FF))
          .bgOpacity(LV_OPA_50)
          .textColor(lv_color_white());
          
    }

    std::string name_;
    Palette palette_;
    Metrics metrics_;
    Fonts fonts_;

    std::unordered_map<std::string, std::unique_ptr<LvglStyle>> styles_;
    inline static std::shared_ptr<LvglTheme> active_ = nullptr;
  };

} // namespace ui
