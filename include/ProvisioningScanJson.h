#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WifiScanController.h"

class ProvisioningScanJson {
 public:
  static constexpr size_t MAX_RESPONSE_BYTES = 1536;
  static bool serialize(const WifiScanSnapshot &snapshot, uint32_t nowMillis,
                        char *destination, size_t capacity, size_t &length);
};
