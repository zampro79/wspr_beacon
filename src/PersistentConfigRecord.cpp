#include "PersistentConfigRecord.h"

#include <string.h>

#include "ConfigValidator.h"

namespace {
constexpr uint8_t MAGIC[4] = {'W', 'S', 'P', 'R'};

void put16(uint8_t *data, uint16_t value) {
  data[0] = static_cast<uint8_t>(value);
  data[1] = static_cast<uint8_t>(value >> 8);
}
void put32(uint8_t *data, uint32_t value) {
  for (uint8_t index = 0; index < 4; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8));
  }
}
uint16_t get16(const uint8_t *data) {
  return static_cast<uint16_t>(data[0]) |
         static_cast<uint16_t>(data[1]) << 8;
}
uint32_t get32(const uint8_t *data) {
  uint32_t value = 0;
  for (uint8_t index = 0; index < 4; ++index) {
    value |= static_cast<uint32_t>(data[index]) << (index * 8);
  }
  return value;
}
uint32_t updateCrc(uint32_t crc, const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return crc;
}
uint32_t recordCrc(const uint8_t *data, size_t payloadLength) {
  uint32_t crc = updateCrc(0xFFFFFFFFUL, data, 14);
  crc = updateCrc(crc, data + PersistentConfigRecord::HEADER_SIZE,
                  payloadLength);
  return crc ^ 0xFFFFFFFFUL;
}
bool makeHeader(ConfigRecordType type, uint8_t payloadVersion,
                size_t payloadLength, uint32_t sequence, uint8_t *output,
                size_t capacity) {
  if (output == nullptr ||
      capacity < PersistentConfigRecord::HEADER_SIZE + payloadLength) {
    return false;
  }
  memcpy(output, MAGIC, sizeof(MAGIC));
  output[4] = PersistentConfigRecord::FORMAT_VERSION;
  output[5] = static_cast<uint8_t>(type);
  output[6] = payloadVersion;
  output[7] = 0;
  put16(output + 8, static_cast<uint16_t>(payloadLength));
  put32(output + 10, sequence);
  put32(output + 14, 0);
  return true;
}
bool validHeader(const uint8_t *data, size_t length, ConfigRecordType type,
                 uint8_t payloadVersion, size_t payloadLength,
                 uint32_t &sequence) {
  if (data == nullptr || length != PersistentConfigRecord::HEADER_SIZE + payloadLength ||
      memcmp(data, MAGIC, sizeof(MAGIC)) != 0 ||
      data[4] != PersistentConfigRecord::FORMAT_VERSION ||
      data[5] != static_cast<uint8_t>(type) ||
      data[6] != payloadVersion || data[7] != 0 ||
      get16(data + 8) != payloadLength ||
      get32(data + 14) != recordCrc(data, payloadLength)) {
    return false;
  }
  sequence = get32(data + 10);
  return true;
}
bool validWifi(const WifiCredentials &credentials) {
  const bool empty = !credentials.hasSsid() && !credentials.hasPassword() &&
                     !credentials.open;
  return empty || (WifiCredentials::validSsid(credentials.ssid) &&
                   (credentials.open ? !credentials.hasPassword()
                                     : WifiCredentials::validPassword(
                                           credentials.password)));
}
}  // namespace

uint32_t PersistentConfigRecord::crc32(const uint8_t *data, size_t length) {
  return updateCrc(0xFFFFFFFFUL, data, length) ^ 0xFFFFFFFFUL;
}

bool PersistentConfigRecord::encodeWspr(const BeaconSettings &settings,
                                        uint32_t sequence, uint8_t *output,
                                        size_t capacity, size_t &length) {
  length = 0;
  if (ConfigValidator::validate(settings) != ConfigValidation::VALID ||
      !makeHeader(ConfigRecordType::WSPR, WSPR_PAYLOAD_VERSION_V3,
                  WSPR_PAYLOAD_SIZE, sequence, output, capacity)) {
    return false;
  }
  uint8_t *payload = output + HEADER_SIZE;
  memset(payload, 0, WSPR_PAYLOAD_SIZE);
  memcpy(payload, settings.callsign, strlen(settings.callsign));
  memcpy(payload + 6, settings.fixedLocator, 4);
  put32(payload + 10, settings.baseFrequencyHz);
  payload[14] = static_cast<uint8_t>(
      static_cast<int8_t>(settings.frequencyOffsetHz));
  payload[15] = static_cast<uint8_t>(settings.band) |
                (settings.powerSave ? 0x08U : 0U) |
                (settings.frequencyMode == WsprFrequencyMode::RANDOM
                     ? 0x80U
                     : 0U);
  put32(payload + 16, static_cast<uint32_t>(settings.si5351Correction));
  payload[20] = settings.powerDbm;
  payload[21] = settings.transmitIntervalMinutes;
  put32(output + 14, recordCrc(output, WSPR_PAYLOAD_SIZE));
  length = HEADER_SIZE + WSPR_PAYLOAD_SIZE;
  return true;
}

bool PersistentConfigRecord::decodeWspr(const uint8_t *data, size_t length,
                                        BeaconSettings &settings,
                                        uint32_t &sequence) {
  uint8_t payloadVersion = 0;
  return decodeWsprWithVersion(data, length, settings, sequence,
                               payloadVersion);
}

bool PersistentConfigRecord::decodeWsprWithVersion(
    const uint8_t *data, size_t length, BeaconSettings &settings,
    uint32_t &sequence, uint8_t &payloadVersion) {
  if (data == nullptr || length != HEADER_SIZE + WSPR_PAYLOAD_SIZE) {
    return false;
  }
  const uint8_t version = data[6];
  if ((version != WSPR_PAYLOAD_VERSION_V1 &&
       version != WSPR_PAYLOAD_VERSION_V2 &&
       version != WSPR_PAYLOAD_VERSION_V3) ||
      !validHeader(data, length, ConfigRecordType::WSPR, version,
                   WSPR_PAYLOAD_SIZE, sequence)) {
    return false;
  }
  const uint8_t *payload = data + HEADER_SIZE;
  BeaconSettings decoded;
  memcpy(decoded.callsign, payload, 6);
  decoded.callsign[6] = '\0';
  memcpy(decoded.fixedLocator, payload + 6, 4);
  decoded.fixedLocator[4] = '\0';
  decoded.baseFrequencyHz = get32(payload + 10);
  if (version == WSPR_PAYLOAD_VERSION_V1) {
    decoded.frequencyOffsetHz = static_cast<int16_t>(get16(payload + 14));
    decoded.band = BandPlan::DEFAULT_BAND;
    decoded.frequencyMode = WsprFrequencyMode::FIXED;
    decoded.powerSave = false;
  } else {
    decoded.frequencyOffsetHz = static_cast<int8_t>(payload[14]);
    const uint8_t packedSelection = payload[15];
    if (version == WSPR_PAYLOAD_VERSION_V3 &&
        (packedSelection & 0x70U) != 0) return false;
    decoded.band = static_cast<WsprBandId>(
        packedSelection & (version == WSPR_PAYLOAD_VERSION_V3 ? 0x07U
                                                               : 0x7FU));
    decoded.powerSave = version == WSPR_PAYLOAD_VERSION_V3 &&
                        (packedSelection & 0x08U) != 0;
    decoded.frequencyMode = (packedSelection & 0x80U) != 0
                                ? WsprFrequencyMode::RANDOM
                                : WsprFrequencyMode::FIXED;
  }
  decoded.si5351Correction = static_cast<int32_t>(get32(payload + 16));
  decoded.powerDbm = payload[20];
  decoded.transmitIntervalMinutes = payload[21];
  if (ConfigValidator::validate(decoded) != ConfigValidation::VALID) return false;
  settings = decoded;
  payloadVersion = version;
  return true;
}

bool PersistentConfigRecord::encodeWifi(const WifiCredentials &credentials,
                                        uint32_t sequence, uint8_t *output,
                                        size_t capacity, size_t &length) {
  length = 0;
  if (!validWifi(credentials) ||
      !makeHeader(ConfigRecordType::WIFI,
                  credentials.open ? WIFI_PAYLOAD_VERSION_V2 : WIFI_PAYLOAD_VERSION,
                  WIFI_PAYLOAD_SIZE, sequence, output, capacity)) {
    return false;
  }
  uint8_t *payload = output + HEADER_SIZE;
  memset(payload, 0, WIFI_PAYLOAD_SIZE);
  const size_t ssidLength = strlen(credentials.ssid);
  const size_t passwordLength = strlen(credentials.password);
  payload[0] = static_cast<uint8_t>(ssidLength);
  payload[1] = static_cast<uint8_t>(passwordLength);
  memcpy(payload + 2, credentials.ssid, ssidLength);
  memcpy(payload + 34, credentials.password, passwordLength);
  put32(output + 14, recordCrc(output, WIFI_PAYLOAD_SIZE));
  length = HEADER_SIZE + WIFI_PAYLOAD_SIZE;
  return true;
}

bool PersistentConfigRecord::decodeWifi(const uint8_t *data, size_t length,
                                        WifiCredentials &credentials,
                                        uint32_t &sequence) {
  if (data == nullptr || length < HEADER_SIZE) return false;
  const uint8_t version = data[6];
  if ((version != WIFI_PAYLOAD_VERSION &&
       version != WIFI_PAYLOAD_VERSION_V2) ||
      !validHeader(data, length, ConfigRecordType::WIFI,
                   version, WIFI_PAYLOAD_SIZE, sequence)) {
    return false;
  }
  const uint8_t *payload = data + HEADER_SIZE;
  const uint8_t ssidLength = payload[0];
  const uint8_t passwordLength = payload[1];
  if (ssidLength > 32 || passwordLength > 64) return false;
  WifiCredentials decoded;
  memcpy(decoded.ssid, payload + 2, ssidLength);
  decoded.ssid[ssidLength] = '\0';
  memcpy(decoded.password, payload + 34, passwordLength);
  decoded.password[passwordLength] = '\0';
  decoded.open = version == WIFI_PAYLOAD_VERSION_V2;
  if (!validWifi(decoded)) return false;
  credentials = decoded;
  return true;
}

bool PersistentConfigRecord::sequenceNewer(uint32_t candidate,
                                           uint32_t reference) {
  return static_cast<int32_t>(candidate - reference) > 0;
}
