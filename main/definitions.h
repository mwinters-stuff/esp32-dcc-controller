#pragma once

#define MSG_WIFI_CONNECTED 1
#define MSG_WIFI_FAILED 2
#define MSG_WIFI_NOT_SAVED 3
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


#define NVS_NAMESPACE "touch_cal"
#define NVS_CALIBRATION_SAVED "cal_saved"
#define NVS_CALIBRATION "calib"

#define NVS_NAMESPACE_WIFI "wifi_creds"
#define NVS_WIFI_SSID "wifi_ssid"
#define NVS_WIFI_PWD  "wifi_pwd"

#define DEFAULT_SCAN_LIST_SIZE 20