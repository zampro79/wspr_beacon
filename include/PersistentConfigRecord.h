#pragma once

#include <stddef.h>
#include <stdint.h>

#include "BeaconSettings.h"
#include "ConfigStorage.h"
#include "WifiCredentials.h"

struct DecodedRecordInfo {
  uint32_t sequence = 0;
  uint8_t slot = 0;
};

class PersistentConfigRecord {
 public:
  static constexpr uint8_t FORMAT_VERSION = 1;
  static constexpr uint8_t WSPR_PAYLOAD_VERSION_V1 = 1;
  static constexpr uint8_t WSPR_PAYLOAD_VERSION_V2 = 2;
  static constexpr uint8_t WSPR_PAYLOAD_VERSION_V3 = 3;
  static constexpr uint8_t WIFI_PAYLOAD_VERSION = 1;
  static constexpr uint8_t WIFI_PAYLOAD_VERSION_V2 = 2;
  static constexpr size_t HEADER_SIZE = 18;
  static constexpr size_t WSPR_PAYLOAD_SIZE = 22;
  static constexpr size_t WIFI_PAYLOAD_SIZE = 98;
  static constexpr size_t WSPR_RECORD_SIZE = HEADER_SIZE + WSPR_PAYLOAD_SIZE;
  static constexpr size_t WIFI_RECORD_SIZE = HEADER_SIZE + WIFI_PAYLOAD_SIZE;
  static constexpr size_t MAX_RECORD_SIZE = HEADER_SIZE + WIFI_PAYLOAD_SIZE;

  static uint32_t crc32(const uint8_t *data, size_t length);
  static bool encodeWspr(const BeaconSettings &settings, uint32_t sequence,
                         uint8_t *output, size_t capacity, size_t &length);
  static bool decodeWspr(const uint8_t *data, size_t length,
                         BeaconSettings &settings, uint32_t &sequence);
  static bool decodeWsprWithVersion(const uint8_t *data, size_t length,
                                    BeaconSettings &settings,
                                    uint32_t &sequence,
                                    uint8_t &payloadVersion);
  static bool encodeWifi(const WifiCredentials &credentials, uint32_t sequence,
                         uint8_t *output, size_t capacity, size_t &length);
  static bool decodeWifi(const uint8_t *data, size_t length,
                         WifiCredentials &credentials, uint32_t &sequence);
  static bool sequenceNewer(uint32_t candidate, uint32_t reference);
};
