#pragma once
#include <lvgl.h>
#include <memory>
#include <string>
#include <LovyanGFX.hpp>
#include <esp_err.h>
#include <esp_log.h>

namespace display {

class Screen : public std::enable_shared_from_this<Screen> {
public:
     virtual ~Screen() = default;

    virtual void show(lv_obj_t* parent = nullptr) {
        lvObj_ = parent ? parent : lv_scr_act();
        lv_obj_clean(lvObj_);
    }

    virtual void cleanUp() {
        if (lvObj_) lv_obj_clean(lvObj_);
    }
    virtual void showError(esp_err_t err){
        ESP_LOGE("ERROR","ESP Error: %s", esp_err_to_name(err));
    }
protected:
lv_obj_t* lvObj_ = nullptr;
};

} 

