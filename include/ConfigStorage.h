#pragma once

#include <stddef.h>
#include <stdint.h>

enum class ConfigRecordType : uint8_t { WSPR = 1, WIFI = 2 };
enum class StorageReadResult : uint8_t { OK, ABSENT, ERROR };

class ConfigStorage {
 public:
  virtual ~ConfigStorage() = default;
  virtual bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
                    size_t capacity, size_t &length) = 0;
  virtual bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
                     size_t length) = 0;
  virtual StorageReadResult readFactoryMarker(uint8_t slot, uint8_t *data,
                                              size_t capacity,
                                              size_t &length) {
    (void)slot; (void)data; (void)capacity; length = 0;
    return StorageReadResult::ERROR;
  }
  virtual bool writeFactoryMarker(uint8_t slot, const uint8_t *data,
                                  size_t length) {
    (void)slot; (void)data; (void)length; return false;
  }
  virtual bool invalidate(ConfigRecordType type, uint8_t slot) {
    (void)type; (void)slot; return false;
  }
  virtual bool clearFactoryMarkers() { return false; }
  virtual StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                              size_t &length) {
    (void)data; (void)capacity; length = 0;
    return StorageReadResult::ERROR;
  }
  virtual bool writeApSetupMarker(const uint8_t *data, size_t length) {
    (void)data; (void)length; return false;
  }
};
