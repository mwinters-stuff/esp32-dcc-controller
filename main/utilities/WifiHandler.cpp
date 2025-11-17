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
#include <mdns.h>
#include <vector>
#include <string>
#include <map>

static void wifi_connect_task(void *pvParameter)
{
  utilities::wifi_credentials_t *creds = (utilities::wifi_credentials_t *)pvParameter;
  creds->wifiHandler->WifiConnectTask(pvParameter);
}

namespace utilities
{
  static const char *TAG = "WifiHandler";

  EventGroupHandle_t WifiHandler::wifi_event_group;

 // --- New static variables used by the continuous mdns task (file-local) ---
  static volatile bool s_mdns_search_running = false;
  static TaskHandle_t s_mdns_search_task = nullptr;


  // Storage for discovered devices
  std::vector<WithrottleDevice> WifiHandler::withrottle_devices;


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
    WifiHandler *self = static_cast<WifiHandler *>(arg);

    ESP_LOGI(TAG, "WifiEventHandler: event_base=%s, event_id=%d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
      ESP_LOGI(TAG, "WifiEventHandler: WIFI_EVENT_STA_DISCONNECTED");
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
      lv_msg_send(MSG_WIFI_FAILED, NULL);
      self->stopMdnsSearchLoop();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
      ESP_LOGI(TAG, "WifiEventHandler: IP_EVENT_STA_GOT_IP");
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
      lv_msg_send(MSG_WIFI_CONNECTED, NULL);
      xTaskCreate([](void *parameters){
        WifiHandler *self = static_cast<WifiHandler *>(parameters);
        vTaskDelay(pdMS_TO_TICKS(1000));
        self->startMdnsSearchLoop(5000 /*interval_ms*/, 2000 /*query_timeout_ms*/, 10 /*max_results*/);
        vTaskDelete(nullptr);

    }, "wifi_init_del", 4096, self, tskIDLE_PRIORITY + 1, nullptr);
      
    }
  }

  void WifiHandler::wifi_event_group_init()
  {
    ESP_LOGI(TAG, "Initializing WiFi event group");
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, this));
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

    connected = bits & WIFI_CONNECTED_BIT;

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

// New: start an mDNS search for _withrottle and populate withrottle_devices
  void WifiHandler::searchMdnsWithrottle(uint32_t timeout_ms /*= 3000*/, size_t max_results /*= 10*/)
  {
    ESP_LOGI(TAG, "Starting mDNS search for service '_withrottle'");
    ESP_ERROR_CHECK(mdns_init());

    // Query for PTR records of the service; service string usually includes proto, e.g. "_withrottle._tcp".
    const char *service = "_withrottle";
    mdns_result_t *results = nullptr;
    esp_err_t err = mdns_query_ptr(service, "_tcp", timeout_ms, max_results, &results);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "mdns_query_ptr('%s') failed: %s", service, esp_err_to_name(err));
      return;
    }

    handleMdnsResults(results);
    mdns_query_results_free(results);
  }

// Background task entry used for continuous searching.
  static void mdns_search_task_fn(void *arg)
  {
    struct TaskParams {
      WifiHandler *self;
      uint32_t interval_ms;
      uint32_t query_timeout_ms;
      size_t max_results;
    };

    TaskParams *p = static_cast<TaskParams *>(arg);
    WifiHandler *self = p->self;
    uint32_t interval = p->interval_ms;
    uint32_t qtimeout = p->query_timeout_ms;
    size_t maxr = p->max_results;

    // loop until stopped
    while (s_mdns_search_running)
    {
      self->searchMdnsWithrottle(qtimeout, maxr);
      // wait interval while allowing task to be cancelled
      for (uint32_t waited = 0; waited < interval && s_mdns_search_running; waited += 200)
      {
        vTaskDelay(pdMS_TO_TICKS(200));
      }
    }

    // cleanup
    s_mdns_search_task = nullptr;
    free(p);
    vTaskDelete(nullptr);
  }

  // Start continuous mDNS search loop. Safe to call repeatedly.
  void WifiHandler::startMdnsSearchLoop(uint32_t interval_ms /*= 5000*/, uint32_t query_timeout_ms /*= 2000*/, size_t max_results /*= 10*/)
  {
    if (s_mdns_search_running)
    {
      ESP_LOGI(TAG, "mDNS search loop already running");
      return;
    }

    // Initialize mdns once; calling mdns_init multiple times returns error
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
      ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
      // still proceed — mdns may be unavailable
    }

    s_mdns_search_running = true;

    // allocate params for task
    struct TaskParams {
      WifiHandler *self;
      uint32_t interval_ms;
      uint32_t query_timeout_ms;
      size_t max_results;
    };

    TaskParams *p = (TaskParams *)malloc(sizeof(TaskParams));
    p->self = this;
    p->interval_ms = interval_ms;
    p->query_timeout_ms = query_timeout_ms;
    p->max_results = max_results;

    BaseType_t ok = xTaskCreate(&mdns_search_task_fn, "mdns_search", 4096, p, tskIDLE_PRIORITY + 1, &s_mdns_search_task);
    if (ok != pdPASS)
    {
      ESP_LOGE(TAG, "Failed to create mdns_search task");
      s_mdns_search_running = false;
      free(p);
    }
  }

  // Stop the continuous mDNS search loop.
  void WifiHandler::stopMdnsSearchLoop()
  {
    if (!s_mdns_search_running)
      return;

    s_mdns_search_running = false;

    // wait briefly for task to exit
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
    while (s_mdns_search_task != nullptr && xTaskGetTickCount() < deadline)
    {
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (s_mdns_search_task != nullptr)
    {
      ESP_LOGW(TAG, "mdns search task did not stop in time");
    }
    else
    {
      ESP_LOGI(TAG, "mdns search loop stopped");
    }
  }

  // New: parse a linked list of mdns_result_t and add devices to the list
  void WifiHandler::handleMdnsResults(mdns_result_t *results)
  {
    for (mdns_result_t *r = results; r != nullptr; r = r->next)
    {
      addWithrottleDeviceFromResult(r);
    }

    ESP_LOGI(TAG, "mDNS: found %u _withrottle device(s)", (unsigned)withrottle_devices.size());
  }

  // New: convert a single mdns_result_t into a WithrottleDevice and append to list
  void WifiHandler::addWithrottleDeviceFromResult(mdns_result_t *r)
  {
    if (!r)
      return;

    WithrottleDevice dev;

    if (r->instance_name)
      dev.instance = r->instance_name;
    if (r->hostname)
      dev.hostname = r->hostname;

    // port
    dev.port = r->port;

    // IP (IPv4)
    if (r->addr && r->addr->addr.type == IPADDR_TYPE_V4)
    {
      esp_ip4_addr_t ipaddr = r->addr->addr.u_addr.ip4;
      dev.ip = std::to_string(esp_ip4_addr1_16(&ipaddr))  + 
        '.' + std::to_string(esp_ip4_addr2_16(&ipaddr)) +
        '.' + std::to_string(esp_ip4_addr3_16(&ipaddr)) +
        '.' + std::to_string(esp_ip4_addr4_16(&ipaddr)) ;
    }
    else if (r->addr && r->addr->addr.type == IPADDR_TYPE_V6)
    {
      dev.ip = "<ipv6>";
    }

    // TXT items
    for (size_t i = 0; i < r->txt_count; ++i)
    {
      if (r->txt && r->txt[i].key)
      {
        std::string key = r->txt[i].key;
        std::string value = r->txt[i].value ? r->txt[i].value : "";
        dev.txt.emplace(key, value);
      }
    }

    ESP_LOGI(TAG,"MDNS Instance: %s", dev.instance.c_str());
    ESP_LOGI(TAG,"MDNS Hostname: %s", dev.hostname.c_str());
    ESP_LOGI(TAG,"MDNS IP Address: %s:%d", dev.ip.c_str(), dev.port);
    for(auto &txt : dev.txt){
      ESP_LOGI(TAG,"MDNS TXT: %s = %s", txt.first.c_str(), txt.second.c_str());
    }

    
        // Ensure only unique hosts are kept: match by hostname (prefer) or IP.
    for (auto &existing : withrottle_devices)
    {
      if (!dev.hostname.empty() && !existing.hostname.empty() && dev.hostname == existing.hostname)
      {
        // update existing entry with freshest info
        existing.instance = dev.instance.empty() ? existing.instance : dev.instance;
        existing.port = dev.port ? dev.port : existing.port;
        existing.ip = dev.ip.empty() ? existing.ip : dev.ip;
        existing.txt = dev.txt.empty() ? existing.txt : dev.txt;
        ESP_LOGI(TAG, "mDNS: updated existing device (hostname): %s", existing.hostname.c_str());
        lv_msg_send(MSG_MDNS_DEVICE_CHANGED, nullptr);
        return;
      }

      if (!dev.ip.empty() && !existing.ip.empty() && dev.ip == existing.ip)
      {
        // update existing entry (IP match)
        existing.instance = dev.instance.empty() ? existing.instance : dev.instance;
        existing.port = dev.port ? dev.port : existing.port;
        existing.hostname = existing.hostname.empty() ? dev.hostname : existing.hostname;
        existing.txt = dev.txt.empty() ? existing.txt : dev.txt;
        ESP_LOGI(TAG, "mDNS: updated existing device (ip): %s", existing.ip.c_str());
        lv_msg_send(MSG_MDNS_DEVICE_CHANGED, nullptr);
        return;
      }
    }

    // not found => append
    withrottle_devices.push_back(std::move(dev));
    lv_msg_send(MSG_MDNS_DEVICE_ADDED, nullptr);
    ESP_LOGI(TAG, "mDNS: adding new device (ip): %s devices: %d", dev.ip.c_str(), (int)withrottle_devices.size());
    
  }

  // New: getter for the discovered devices
  std::vector<WithrottleDevice> WifiHandler::getWithrottleDevices() const
  {
    return withrottle_devices;
  }  
}
