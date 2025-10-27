#pragma once

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9488     _panel_instance;
  lgfx::Bus_SPI           _bus_instance;
  lgfx::Light_PWM         _light_instance;
  lgfx::Touch_XPT2046     _touch_instance;


public:
  static const uint32_t screenWidth  = 320;   // Portrait width
  static const uint32_t screenHeight = 480;  // Portrait height

  LGFX(void)
  {
    { // SPI bus config
      auto cfg = _bus_instance.config();

      // SPI bus pins
      cfg.spi_host   = SPI2_HOST;  // Use FSPI on ESP32-S3
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;   // 40 MHz for LCD
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 12;  // SCK
      cfg.pin_mosi = 11;  // MOSI
      cfg.pin_miso = 13;  // MISO (used by XPT2046)
      cfg.pin_dc   = 8;   // DC/RS (LCD)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // LCD panel config
      auto cfg = _panel_instance.config();

      cfg.pin_cs   = 10;  // LCD CS
      cfg.pin_rst  = 16;   // LCD RESET
      cfg.pin_busy = -1;

      cfg.panel_width  = screenWidth;
      cfg.panel_height = screenHeight;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;

      cfg.readable     = false;
      cfg.invert       = false;
      cfg.rgb_order    = false;
      cfg.dlen_16bit   = false;
      cfg.bus_shared   = true;   // Shared with touch

      _panel_instance.config(cfg);
    }

    { // Backlight (optional)
      auto cfg = _light_instance.config();
      cfg.pin_bl = 4;   // Set your BL pin if you have one
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    { // Touch config
      auto cfg = _touch_instance.config();

      cfg.spi_host    = SPI2_HOST;
      cfg.freq        = 2000000;  // 2 MHz for touch
      cfg.pin_sclk    = 12;
      cfg.pin_mosi    = 11;
      cfg.pin_miso    = 13;
      cfg.pin_cs      = 9;        // T_CS
      // cfg.pin_irq     = 6;        // T_IRQ

      cfg.x_min = 200;
      cfg.x_max = 3900;
      cfg.y_min = 200;
      cfg.y_max = 3900;

      cfg.bus_shared = true;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
