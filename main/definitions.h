#pragma once

#include "ui/lv_msg.h"
#include <cstdint>

#define MSG_WIFI_CONNECTED 1
#define MSG_WIFI_FAILED 2
#define MSG_WIFI_NOT_SAVED 3
#define MSG_RESHOW_WIFI_CONNECT_SCREEN 4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define MSG_MDNS_DEVICE_ADDED 10
#define MSG_MDNS_DEVICE_CHANGED 11
#define MSG_CONNECTING_TO_DCC_SERVER 12
#define MSG_DCC_CONNECTION_SUCCESS 13
#define MSG_DCC_CONNECTION_FAILED 14
#define MSG_DCC_DISCONNECTED 15

#define MSG_DCC_ROSTER_LIST_RECEIVED 20
#define MSG_DCC_TURNOUT_LIST_RECEIVED 21
#define MSG_DCC_ROUTE_LIST_RECEIVED 22
#define MSG_DCC_TURNTABLE_LIST_RECEIVED 23
#define MSG_DCC_TURNOUT_CHANGED 24
#define MSG_DCC_TRACK_POWER_CHANGED 25
#define MSG_DCC_TURNTABLE_CHANGED 26

#define NVS_NAMESPACE "touch_cal"
#define NVS_CALIBRATION_SAVED "cal_saved"
#define NVS_CALIBRATION "calib"

#define NVS_NAMESPACE_WIFI "wifi_creds"
#define NVS_WIFI_SSID "wifi_ssid"
#define NVS_WIFI_PWD "wifi_pwd"

#define NVS_NAMESPACE_DCC "dcc_conn"
#define NVS_DCC_SAVED "saved"
#define NVS_DCC_INSTANCE "instance"
#define NVS_DCC_HOSTNAME "hostname"
#define NVS_DCC_IP "ip"
#define NVS_DCC_PORT "port"

#define DEFAULT_SCAN_LIST_SIZE 20

enum class WifiFailedSource : uint8_t {
  Unknown = 0,
  ConnectAttempt = 1,
  Disconnected = 2,
};

struct WifiFailedPayload {
  WifiFailedSource source;
  bool suppressGlobalPopup;
};