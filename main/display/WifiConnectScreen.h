#include "Screen.h"
#include "ui/LvglButton.h"
#include "ui/LvglLabel.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <string>

namespace display
{
    using namespace ui;


    class WifiConnectScreen : public Screen, public std::enable_shared_from_this<WifiConnectScreen>
    {
    public:
        static std::shared_ptr<WifiConnectScreen> instance()
        {
            static std::shared_ptr<WifiConnectScreen> s;
            if (!s)
                s.reset(new WifiConnectScreen());
            return s;
        }
        ~WifiConnectScreen() override = default;

        void setSSID(const std::string &ssid);

        void show(lv_obj_t *parent = nullptr, std::weak_ptr<Screen> parentScreen = std::weak_ptr<Screen>{}) override;
        void cleanUp() override;

        static void wifi_connect_task(void *pvParameter);

        WifiConnectScreen(const WifiConnectScreen &) = delete;
        WifiConnectScreen &operator=(const WifiConnectScreen &) = delete;

        lv_obj_t *status_label_;
        lv_obj_t *spinner_;
        lv_obj_t *ssid_label_;
        lv_obj_t *pwd_textarea_;
        lv_obj_t *connect_btn_;
        lv_obj_t *cancel_btn_;
        lv_obj_t *keyboard_;

        typedef struct
        {
            char ssid[33];
            char password[65];
        } wifi_credentials_t;

        std::shared_ptr<wifi_credentials_t> creds;
    private:


        std::string ssid_;

        static void connect_btn_event_handler(lv_event_t *e);
        static void cancel_btn_event_handler(lv_event_t *e);

        static void textarea_event_handler(lv_event_t *e);
        static void keyboard_event_cb(lv_event_t * e);
        void createScreen();
        void onConnectPressed();

    protected:
        WifiConnectScreen() = default;
    };
} // namespace display