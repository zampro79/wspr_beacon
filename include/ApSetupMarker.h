#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigStorage.h"

enum class ApSetupMarkerState : uint8_t {
  ABSENT, PENDING, CONSUMED, CORRUPT, STORAGE_ERROR
};
enum class ApSetupMarkerResult : uint8_t {
  OK, ALREADY_PENDING, STORAGE_ERROR, VERIFICATION_FAILED, CORRUPT
};

class ApSetupMarker {
 public:
  static constexpr size_t SIZE = 16;
  explicit ApSetupMarker(ConfigStorage &storage) : storage_(storage) {}
  ApSetupMarkerState read() const;
  ApSetupMarkerResult request();
  ApSetupMarkerResult consume();
  // Factory Reset has its own durable transaction marker. Call only after
  // that marker is verified; a failed cancellation must retain that marker.
  ApSetupMarkerResult cancelForFactoryReset();

 private:
  ConfigStorage &storage_;
  ApSetupMarkerResult writeAndVerify(uint8_t state);
};
