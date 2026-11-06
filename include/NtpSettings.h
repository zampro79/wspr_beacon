#pragma once

#include <stdint.h>

constexpr uint32_t NTP_REQUEST_TIMEOUT_MS = 30000UL;
constexpr uint32_t NTP_REFERENCE_TIMEOUT_MS = 3600000UL;
constexpr uint32_t ESP32_SNTP_SYNC_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr uint32_t ESP32_NTP_RECOVERY_INITIAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t ESP32_NTP_RECOVERY_MAX_MS = 15UL * 60UL * 1000UL;

static_assert(ESP32_SNTP_SYNC_INTERVAL_MS < NTP_REFERENCE_TIMEOUT_MS,
              "SNTP must renew before the NTP reference expires");
static_assert(ESP32_NTP_RECOVERY_INITIAL_MS > NTP_REQUEST_TIMEOUT_MS,
              "Recovery must not restart SNTP at the diagnostic timeout");
static_assert(ESP32_NTP_RECOVERY_INITIAL_MS <= ESP32_NTP_RECOVERY_MAX_MS,
              "NTP recovery backoff must be monotonic");
