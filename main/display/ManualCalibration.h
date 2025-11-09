#pragma once
#include <lvgl.h>
#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <memory>

namespace display {

    enum calibrateState{
        notCalibrated,
        calibrate,
        showScreen,
        calibrated
    };

class ManualCalibration : public Screen {
public:
    // Get the singleton instance. Optional parent only used on first creation.
    static std::shared_ptr<ManualCalibration> instance() {
        static std::shared_ptr<ManualCalibration> s;
        if(!s) s.reset(new ManualCalibration());
        return s;
    }



    virtual ~ManualCalibration() = default;

    // existing public API
    virtual void show(lv_obj_t* parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
    calibrateState loadCalibrationFromNVS();
    virtual void cleanUp() override;
    void calibrate();
    void addBackButton(std::weak_ptr<Screen> screenToShow);

    // non-copyable, non-movable
   ManualCalibration(const ManualCalibration&) = delete;
    ManualCalibration& operator=(const ManualCalibration&) = delete;

    ManualCalibration& operator=(ManualCalibration&&) = delete;

protected:
    ManualCalibration() {}; // protected ctor

private:
    std::shared_ptr<ui::LvglLabel> lbl_title;
    std::shared_ptr<ui::LvglLabel> lbl_sub_title;
    std::shared_ptr<ui::LvglButton> btn_start;
    std::shared_ptr<ui::LvglButton> btn_back;

    uint16_t parameters[8];

    void rebootToCalibrate();
    void save_to_nvs(calibrateState state);
    calibrateState load_from_nvs();


};

} // namespace display
