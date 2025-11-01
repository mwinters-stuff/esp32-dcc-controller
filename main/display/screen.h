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
    Screen() {}
    virtual ~Screen() = default;

    virtual void show(lv_obj_t* parent = nullptr) = 0;
    virtual void cleanUp() {}

    virtual void showError(esp_err_t err){
        ESP_LOGE("ERROR","ESP Error: %s", esp_err_to_name(err));
    }
protected:

};

} 

