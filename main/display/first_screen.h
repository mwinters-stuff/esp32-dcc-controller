#pragma once
#include <lvgl.h>
#include "screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <memory>


namespace display {

class FirstScreen : public Screen {
public:
    // Get the singleton instance
    static std::shared_ptr<FirstScreen> instance() {
        static std::shared_ptr<FirstScreen> s;
        if(!s) s.reset(new FirstScreen());
        return s;
    }

    virtual ~FirstScreen() = default;

    // existing public API
    virtual void show(lv_obj_t* parent = nullptr) override;
    virtual void cleanUp() override;

    // non-copyable, non-movable
    FirstScreen(const FirstScreen&) = delete;
    FirstScreen& operator=(const FirstScreen&) = delete;

protected:
    FirstScreen() {}; // keep the ctor protected so instance() controls creation
private:
    const char *TAG = "FIRST_SCREEN";

      std::shared_ptr<ui::LvglLabel> lbl_title;
      std::shared_ptr<ui::LvglButton> btn_connect;
      std::shared_ptr<ui::LvglButton> btn_wifi_scan;
      std::shared_ptr<ui::LvglButton> btn_cal;

      lgfx::LGFX_Device *gfx_device;

};

};// namespace display

