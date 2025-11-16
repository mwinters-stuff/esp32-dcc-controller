#pragma once
#include <memory>

#include <esp_wifi.h>

namespace utilities
{

  class WifiHandler;
  typedef struct
  {
    char ssid[33];
    char password[65];
    std::shared_ptr<WifiHandler> wifiHandler;
  } wifi_credentials_t;

  class WifiHandler : public std::enable_shared_from_this<WifiHandler>
  {
  public:
    static std::shared_ptr<WifiHandler> instance()
    {
      static std::shared_ptr<WifiHandler> s;
      if (!s)
        s.reset(new WifiHandler());
      return s;
    }

    ~WifiHandler() = default;

    WifiHandler(const WifiHandler &) = delete;
    WifiHandler &operator=(const WifiHandler &) = delete;

    void init_wifi();
    void wifi_event_group_init();

    esp_err_t saveConfiguration();
    void loadAndConnect();

    static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    static EventGroupHandle_t wifi_event_group;

    void WifiConnectTask(void *pvParameter);
    void create_wifi_connect_task(const char *ssid, const char *password);

    std::shared_ptr<wifi_credentials_t> creds;
    void noConnectionSaved();

    bool isConnected();

    std::string getIpAddress() const;

  protected:
    bool connected = false;

    WifiHandler() = default;
  };

}