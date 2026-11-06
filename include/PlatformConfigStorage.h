#pragma once

#include "ConfigStorage.h"

#if defined(ARDUINO_ARCH_ESP8266)

class Esp8266ConfigStorage final : public ConfigStorage {
 public:
  bool begin();
  void end();
  bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
            size_t capacity, size_t &length) override;
  bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) override;
  StorageReadResult readFactoryMarker(uint8_t slot, uint8_t *data,
                                      size_t capacity, size_t &length) override;
  bool writeFactoryMarker(uint8_t slot, const uint8_t *data,
                          size_t length) override;
  bool invalidate(ConfigRecordType type, uint8_t slot) override;
  bool clearFactoryMarkers() override;
  StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                      size_t &length) override;
  bool writeApSetupMarker(const uint8_t *data, size_t length) override;

 private:
  static constexpr size_t STORAGE_SIZE = 512;
  bool initialized_ = false;
};

using PlatformConfigStorage = Esp8266ConfigStorage;

#elif defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>

class Esp32ConfigStorage final : public ConfigStorage {
 public:
  bool begin();
  void end();
  bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
            size_t capacity, size_t &length) override;
  bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) override;
  StorageReadResult readFactoryMarker(uint8_t slot, uint8_t *data,
                                      size_t capacity, size_t &length) override;
  bool writeFactoryMarker(uint8_t slot, const uint8_t *data,
                          size_t length) override;
  bool invalidate(ConfigRecordType type, uint8_t slot) override;
  bool clearFactoryMarkers() override;
  StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                      size_t &length) override;
  bool writeApSetupMarker(const uint8_t *data, size_t length) override;

 private:
  Preferences preferences_;
  bool initialized_ = false;
};

using PlatformConfigStorage = Esp32ConfigStorage;

#endif
