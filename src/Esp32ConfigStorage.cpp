#include "PlatformConfigStorage.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <string.h>

#include "PersistentConfigRecord.h"
#include "FactoryResetCore.h"
#include "ApSetupMarker.h"

namespace {
constexpr char NAMESPACE[] = "wspr_cfg";
const char *recordKey(ConfigRecordType type, uint8_t slot) {
  if (type == ConfigRecordType::WSPR) return slot == 0 ? "wspr0" : "wspr1";
  return slot == 0 ? "wifi0" : "wifi1";
}
const char *factoryKey(uint8_t slot) {
  return slot == 0 ? "factory0" : "factory1";
}
size_t recordSize(ConfigRecordType type) {
  return type == ConfigRecordType::WSPR
             ? PersistentConfigRecord::WSPR_RECORD_SIZE
             : PersistentConfigRecord::WIFI_RECORD_SIZE;
}
bool validType(ConfigRecordType type) {
  return type == ConfigRecordType::WSPR || type == ConfigRecordType::WIFI;
}
}  // namespace

bool Esp32ConfigStorage::begin() {
  initialized_ = preferences_.begin(NAMESPACE, false);
  return initialized_;
}

void Esp32ConfigStorage::end() {
  if (initialized_) preferences_.end();
  initialized_ = false;
}

bool Esp32ConfigStorage::read(ConfigRecordType type, uint8_t slot,
                              uint8_t *data, size_t capacity, size_t &length) {
  length = 0;
  if (!validType(type) || !initialized_ || slot > 1 || data == nullptr) {
    return false;
  }
  const char *key = recordKey(type, slot);
  const size_t storedLength = preferences_.getBytesLength(key);
  if (storedLength == 0 || storedLength > capacity) return false;
  const size_t bytesRead = preferences_.getBytes(key, data, storedLength);
  if (bytesRead != storedLength) return false;
  length = bytesRead;
  return true;
}

bool Esp32ConfigStorage::write(ConfigRecordType type, uint8_t slot,
                               const uint8_t *data, size_t length) {
  if (!validType(type)) return false;
  const size_t expected = recordSize(type);
  if (!initialized_ || slot > 1 || data == nullptr || length != expected) {
    return false;
  }
  const char *key = recordKey(type, slot);
  uint8_t existing[PersistentConfigRecord::MAX_RECORD_SIZE];
  const size_t existingLength = preferences_.getBytesLength(key);
  if (existingLength == length &&
      preferences_.getBytes(key, existing, sizeof(existing)) == length &&
      memcmp(existing, data, length) == 0) {
    return true;
  }
  return preferences_.putBytes(key, data, length) == length;
}

StorageReadResult Esp32ConfigStorage::readFactoryMarker(
    uint8_t slot, uint8_t *data, size_t capacity, size_t &length) {
  length = 0;
  if (!initialized_ || slot > 1 || data == nullptr) {
    return StorageReadResult::ERROR;
  }
  const size_t storedLength = preferences_.getBytesLength(factoryKey(slot));
  if (storedLength == 0) return StorageReadResult::ABSENT;
  if (storedLength > capacity) return StorageReadResult::ERROR;
  if (preferences_.getBytes(factoryKey(slot), data, storedLength) !=
      storedLength) return StorageReadResult::ERROR;
  length = storedLength;
  return StorageReadResult::OK;
}

bool Esp32ConfigStorage::writeFactoryMarker(uint8_t slot,
                                             const uint8_t *data,
                                             size_t length) {
  if (!initialized_ || slot > 1 || data == nullptr ||
      length != FactoryResetCore::MARKER_SIZE) return false;
  uint8_t existing[FactoryResetCore::MARKER_SIZE];
  if (preferences_.getBytesLength(factoryKey(slot)) == length &&
      preferences_.getBytes(factoryKey(slot), existing, sizeof(existing)) ==
          length &&
      memcmp(existing, data, length) == 0) return true;
  return preferences_.putBytes(factoryKey(slot), data, length) == length;
}

bool Esp32ConfigStorage::invalidate(ConfigRecordType type, uint8_t slot) {
  if (!validType(type) || slot > 1) return false;
  uint8_t invalid[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
  return write(type, slot, invalid, recordSize(type));
}

bool Esp32ConfigStorage::clearFactoryMarkers() {
  if (!initialized_) return false;
  bool ok = true;
  for (uint8_t slot = 0; slot < 2; ++slot) {
    const char *key = factoryKey(slot);
    if (preferences_.isKey(key)) ok = preferences_.remove(key) && ok;
  }
  return ok;
}

StorageReadResult Esp32ConfigStorage::readApSetupMarker(
    uint8_t *data, size_t capacity, size_t &length) {
  length = 0;
  if (!initialized_ || data == nullptr || capacity < ApSetupMarker::SIZE)
    return StorageReadResult::ERROR;
  const size_t storedLength = preferences_.getBytesLength("ap_setup");
  if (storedLength == 0)
    return preferences_.isKey("ap_setup") ? StorageReadResult::OK
                                           : StorageReadResult::ABSENT;
  if (storedLength > capacity) return StorageReadResult::ERROR;
  if (preferences_.getBytes("ap_setup", data, storedLength) != storedLength)
    return StorageReadResult::ERROR;
  length = storedLength;
  return StorageReadResult::OK;
}

bool Esp32ConfigStorage::writeApSetupMarker(const uint8_t *data,
                                             size_t length) {
  if (!initialized_ || data == nullptr || length != ApSetupMarker::SIZE)
    return false;
  return preferences_.putBytes("ap_setup", data, length) == length;
}

#endif
