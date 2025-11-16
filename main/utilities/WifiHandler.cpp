#include "WifiHandler.h"
#include "definitions.h"
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_task_wdt.h>
#include <freertos/event_groups.h>
#include <lvgl.h>
#include <lwip/inet.h>
#include <memory>
#include <nvs_flash.h>
#include <nvs_handle.hpp>

static void wifi_connect_task(void *pvParameter)
{
  utilities::wifi_credentials_t *creds = (utilities::wifi_credentials_t *)pvParameter;
  creds->wifiHandler->WifiConnectTask(pvParameter);
}

namespace utilities
{
  static const char *TAG = "WifiHandler";

  EventGroupHandle_t WifiHandler::wifi_event_group;

  void WifiHandler::init_wifi()
  {
    ESP_LOGI(TAG, "Initializing WiFi");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_event_group_init();

    loadAndConnect();
  }

  void WifiHandler::wifi_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
  {
    ESP_LOGI(TAG, "WifiEventHandler: event_base=%s, event_id=%d",
             event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
      ESP_LOGI(TAG, "WifiEventHandler: WIFI_EVENT_STA_DISCONNECTED");
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
      lv_msg_send(MSG_WIFI_FAILED, NULL);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
      ESP_LOGI(TAG, "WifiEventHandler: IP_EVENT_STA_GOT_IP");
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
      lv_msg_send(MSG_WIFI_CONNECTED, NULL);
    }
  }

  void WifiHandler::wifi_event_group_init()
  {
    ESP_LOGI(TAG, "Initializing WiFi event group");
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
  }

  esp_err_t WifiHandler::saveConfiguration()
  {
    esp_err_t err;
    if (!creds)
    {
      ESP_LOGI(TAG, "creds is null");
      return ESP_FAIL;
    }

    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_WIFI, NVS_READWRITE, &err);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
      return err;
    }

    err = handle->set_string(NVS_WIFI_SSID, creds->ssid);
    if (err != ESP_OK)
    {
      return err;
    }

    err = handle->set_string(NVS_WIFI_PWD, creds->password);
    if (err != ESP_OK)
    {
      return err;
    }

    err = handle->commit();
    if (err != ESP_OK)
    {
      return err;
    }

    return ESP_OK;
  }

  void WifiHandler::loadAndConnect()
  {
    esp_err_t err;

    std::unique_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle(NVS_NAMESPACE_WIFI, NVS_READONLY, &err);
    creds = std::make_shared<wifi_credentials_t>();
    creds->wifiHandler = shared_from_this();

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
      noConnectionSaved();
      return;
    }

    err = handle->get_string(NVS_WIFI_SSID, creds->ssid, sizeof(creds->ssid));
    if (err != ESP_OK)
    {
      noConnectionSaved();
      return;
    }

    err = handle->get_string(NVS_WIFI_PWD, creds->password, sizeof(creds->password));
    if (err != ESP_OK)
    {
      noConnectionSaved();
      return;
    }

    xTaskCreate(&::wifi_connect_task, "wifi_connect_task", 4096, creds.get(), 5, nullptr);
  }

  void WifiHandler::noConnectionSaved()
  {
    lv_msg_send(MSG_WIFI_NOT_SAVED, NULL);
  }

  void WifiHandler::WifiConnectTask(void *pvParameter)
  {

    connected = false;
    wifi_credentials_t *creds = (wifi_credentials_t *)pvParameter;

    ESP_LOGI(TAG, "Connecting to SSID: %s", creds->ssid);

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, creds->ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, creds->password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(pdMS_TO_TICKS(100));

    // Start connection
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "wifi connect failed: %s", esp_err_to_name(err));
      lv_msg_send(MSG_WIFI_FAILED, NULL);
      vTaskDelete(NULL);
      return;
    }

    // Wait for result via events
    // Optional: timeout (e.g., 10s)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
    {
      lv_msg_send(MSG_WIFI_CONNECTED, NULL);
      connected = true;
    }
    else
    {
      lv_msg_send(MSG_WIFI_FAILED, NULL);
    }

    vTaskDelete(NULL);
  }

  void WifiHandler::create_wifi_connect_task(const char *ssid, const char *password)
  {
    creds = std::make_shared<utilities::wifi_credentials_t>();
    strncpy(creds->ssid, ssid, sizeof(creds->ssid));
    strncpy(creds->password, password, sizeof(creds->password));
    creds->wifiHandler = shared_from_this();

    xTaskCreate(&::wifi_connect_task, "wifi_connect_task", 4096, creds.get(), 5, nullptr);
  }

  bool WifiHandler::isConnected()
  {
    return connected;
  }

  std::string WifiHandler::getIpAddress() const
  {
    // If your class stores esp_netif_t* station_netif, use that instead of getting by key.
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif)
      return {};

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
      char buf[16];
      const ip4_addr_t *ip4 = reinterpret_cast<const ip4_addr_t *>(&ip_info.ip);
      ip4addr_ntoa_r(ip4, buf, sizeof(buf));
      return std::string(buf);
    }
    return {};
  }
}
