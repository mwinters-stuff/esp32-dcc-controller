#pragma once
#include "screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"

namespace display{
class CalibrateScreen : public Screen {
public:
    CalibrateScreen(std::shared_ptr<Screen> previous)
        : previousScreen(previous) {}

    void show(lv_obj_t* parent = nullptr) override;
    void cleanUp() override;
    void runTouchCalibration();

    std::function<void()> onExit;
private:
    std::weak_ptr<Screen> previousScreen;
    std::shared_ptr<ui::LvglLabel> lbl_title;
    std::shared_ptr<ui::LvglLabel> lbl_sub_title;
    std::shared_ptr<ui::LvglButton> btn_save;
    std::shared_ptr<ui::LvglButton> btn_cancel;
    uint16_t parameters[8];
    const char* TAG = "CalibrateScreen";

    
};

};

