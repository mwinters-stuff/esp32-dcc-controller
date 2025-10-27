#pragma once
#include "screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"


namespace display{
class CalibrateScreen;


  class FirstScreen: public Screen{
    public:
      using Screen::Screen;
      virtual ~FirstScreen(){
        cleanUp();
      }
      void show(lv_obj_t * parent = nullptr) override;
      void cleanUp() override;
    private:
      const char *TAG = "FIRST_SCREEN";

      std::shared_ptr<ui::LvglLabel> lbl_title;
      std::shared_ptr<ui::LvglButton> btn_connect;
      std::shared_ptr<ui::LvglButton> btn_wifi_scan;
      std::shared_ptr<ui::LvglButton> btn_cal;

      lgfx::LGFX_Device *gfx_device;

  };

};

