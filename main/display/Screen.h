#pragma once
#include <LovyanGFX.hpp>
#include <esp_err.h>
#include <esp_log.h>
#include <lvgl.h>
#include <memory>
#include <string>

namespace display
{

    class Screen : public std::enable_shared_from_this<Screen>
    {
    public:
        virtual ~Screen() = default;

        virtual void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{})
        {
            lvObj_ = parent ? parent : lv_scr_act();
            lv_obj_clean(lvObj_);
            if(!parentScreen.expired()){
                parentScreen_ = parentScreen;
            }
        }

        virtual void cleanUp()
        {
            if (lvObj_)
                lv_obj_clean(lvObj_);
        }

        virtual void showError(esp_err_t err)
        {
            ESP_LOGE("ERROR", "ESP Error: %s", esp_err_to_name(err));
        }

        std::weak_ptr<Screen> parentScreen_;
    protected:
        lv_obj_t *lvObj_ = nullptr;
    };

} // namespace display
