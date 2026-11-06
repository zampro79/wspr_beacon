#include "PlatformConfigStorage.h"

#if defined(ARDUINO_ARCH_ESP8266)

#include <EEPROM.h>

#include "PersistentConfigRecord.h"
#include "FactoryResetCore.h"
#include "ApSetupMarker.h"

namespace {
constexpr size_t WSPR_RECORD_SIZE = PersistentConfigRecord::WSPR_RECORD_SIZE;
constexpr size_t WIFI_RECORD_SIZE = PersistentConfigRecord::WIFI_RECORD_SIZE;
constexpr size_t WIFI_BASE = WSPR_RECORD_SIZE * 2;
constexpr size_t FACTORY_BASE = WIFI_BASE + WIFI_RECORD_SIZE * 2;
constexpr size_t AP_SETUP_BASE = FACTORY_BASE + FactoryResetCore::MARKER_SIZE * 2;
static_assert(WSPR_RECORD_SIZE == 40 && WIFI_BASE == 80,
              "ESP8266 WSPR v2 must preserve the v1 EEPROM layout");
static_assert(FACTORY_BASE + FactoryResetCore::MARKER_SIZE * 2 <= 512,
              "ESP8266 configuration layout exceeds EEPROM area");
static_assert(AP_SETUP_BASE == 344 && AP_SETUP_BASE + ApSetupMarker::SIZE <= 512,
              "ESP8266 AP setup marker overlaps configuration or exceeds EEPROM");

size_t recordSize(ConfigRecordType type) {
  return type == ConfigRecordType::WSPR ? WSPR_RECORD_SIZE : WIFI_RECORD_SIZE;
}
size_t recordOffset(ConfigRecordType type, uint8_t slot) {
  return type == ConfigRecordType::WSPR
             ? static_cast<size_t>(slot) * WSPR_RECORD_SIZE
             : WIFI_BASE + static_cast<size_t>(slot) * WIFI_RECORD_SIZE;
}
bool validType(ConfigRecordType type) {
  return type == ConfigRecordType::WSPR || type == ConfigRecordType::WIFI;
}
}  // namespace

bool Esp8266ConfigStorage::begin() {
  EEPROM.begin(STORAGE_SIZE);
  initialized_ = EEPROM.getConstDataPtr() != nullptr;
  return initialized_;
}

void Esp8266ConfigStorage::end() {
  if (initialized_) EEPROM.end();
  initialized_ = false;
}

bool Esp8266ConfigStorage::read(ConfigRecordType type, uint8_t slot,
                                uint8_t *data, size_t capacity,
                                size_t &length) {
  length = 0;
  if (!validType(type)) return false;
  const size_t expected = recordSize(type);
  if (!initialized_ || slot > 1 || data == nullptr || capacity < expected) {
    return false;
  }
  const size_t offset = recordOffset(type, slot);
  bool erased = true;
  for (size_t index = 0; index < expected; ++index) {
    data[index] = EEPROM.read(offset + index);
    if (data[index] != 0xFF) erased = false;
  }
  if (erased) return false;
  length = expected;
  return true;
}

bool Esp8266ConfigStorage::write(ConfigRecordType type, uint8_t slot,
                                 const uint8_t *data, size_t length) {
  if (!validType(type)) return false;
  const size_t expected = recordSize(type);
  if (!initialized_ || slot > 1 || data == nullptr || length != expected) {
    return false;
  }
  const size_t offset = recordOffset(type, slot);
  bool changed = false;
  for (size_t index = 0; index < length; ++index) {
    if (EEPROM.read(offset + index) != data[index]) {
      EEPROM.write(offset + index, data[index]);
      changed = true;
    }
  }
  return !changed || EEPROM.commit();
}

StorageReadResult Esp8266ConfigStorage::readFactoryMarker(
    uint8_t slot, uint8_t *data, size_t capacity, size_t &length) {
  length = 0;
  if (!initialized_ || slot > 1 || data == nullptr ||
      capacity < FactoryResetCore::MARKER_SIZE) {
    return StorageReadResult::ERROR;
  }
  const size_t offset = FACTORY_BASE +
                        static_cast<size_t>(slot) * FactoryResetCore::MARKER_SIZE;
  bool erased = true;
  for (size_t index = 0; index < FactoryResetCore::MARKER_SIZE; ++index) {
    data[index] = EEPROM.read(offset + index);
    if (data[index] != 0xFF) erased = false;
  }
  if (erased) return StorageReadResult::ABSENT;
  length = FactoryResetCore::MARKER_SIZE;
  return StorageReadResult::OK;
}

bool Esp8266ConfigStorage::writeFactoryMarker(uint8_t slot,
                                               const uint8_t *data,
                                               size_t length) {
  if (!initialized_ || slot > 1 || data == nullptr ||
      length != FactoryResetCore::MARKER_SIZE) return false;
  const size_t offset = FACTORY_BASE +
                        static_cast<size_t>(slot) * FactoryResetCore::MARKER_SIZE;
  bool changed = false;
  for (size_t index = 0; index < length; ++index) {
    if (EEPROM.read(offset + index) != data[index]) {
      EEPROM.write(offset + index, data[index]);
      changed = true;
    }
  }
  return !changed || EEPROM.commit();
}

bool Esp8266ConfigStorage::invalidate(ConfigRecordType type, uint8_t slot) {
  if (!validType(type) || slot > 1) return false;
  uint8_t invalid[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
  return write(type, slot, invalid, recordSize(type));
}

bool Esp8266ConfigStorage::clearFactoryMarkers() {
  if (!initialized_) return false;
  bool changed = false;
  for (size_t index = 0; index < FactoryResetCore::MARKER_SIZE * 2U; ++index) {
    if (EEPROM.read(FACTORY_BASE + index) != 0xFF) {
      EEPROM.write(FACTORY_BASE + index, 0xFF);
      changed = true;
    }
  }
  return !changed || EEPROM.commit();
}

StorageReadResult Esp8266ConfigStorage::readApSetupMarker(
    uint8_t *data, size_t capacity, size_t &length) {
  length = 0;
  if (!initialized_ || data == nullptr || capacity < ApSetupMarker::SIZE)
    return StorageReadResult::ERROR;
  bool erased = true;
  for (size_t index = 0; index < ApSetupMarker::SIZE; ++index) {
    data[index] = EEPROM.read(AP_SETUP_BASE + index);
    if (data[index] != 0xFF) erased = false;
  }
  if (erased) return StorageReadResult::ABSENT;
  length = ApSetupMarker::SIZE;
  return StorageReadResult::OK;
}

bool Esp8266ConfigStorage::writeApSetupMarker(const uint8_t *data,
                                               size_t length) {
  if (!initialized_ || data == nullptr || length != ApSetupMarker::SIZE)
    return false;
  bool changed = false;
  for (size_t index = 0; index < length; ++index) {
    if (EEPROM.read(AP_SETUP_BASE + index) != data[index]) {
      EEPROM.write(AP_SETUP_BASE + index, data[index]);
      changed = true;
    }
  }
  return !changed || EEPROM.commit();
}

#endif
