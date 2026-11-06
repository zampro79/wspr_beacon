#pragma once

#include "BoardConfig.h"

#if WSPR_HAS_WIFI && __has_include("LocalWifiConfig.h")
  #include "LocalWifiConfig.h"
#else
  #define WSPR_WIFI_ENABLED false
  #define WSPR_WIFI_SSID ""
  #define WSPR_WIFI_PASSWORD ""
#endif

constexpr bool WIFI_LOCAL_ENABLED = WSPR_WIFI_ENABLED;
constexpr const char *WIFI_LOCAL_SSID = WSPR_WIFI_SSID;
constexpr const char *WIFI_LOCAL_PASSWORD = WSPR_WIFI_PASSWORD;
constexpr const char *NTP_SERVER_PRIMARY = "pool.ntp.org";
constexpr const char *NTP_SERVER_SECONDARY = "time.cloudflare.com";
