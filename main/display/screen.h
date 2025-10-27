#pragma once
#include <lvgl.h>
#include <memory>
#include <string>
#include <LovyanGFX.hpp>

namespace display {

class Screen : public std::enable_shared_from_this<Screen> {
public:
    Screen() {}
    virtual ~Screen() = default;

    virtual void show(lv_obj_t* parent = nullptr) = 0;
    virtual void cleanUp() {}

protected:

};

} 

