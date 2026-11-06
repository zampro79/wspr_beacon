#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WifiCredentials.h"

enum class ProvisioningWifiParseResult : uint8_t {
  OK, MALFORMED, UNKNOWN_FIELD, DUPLICATE_FIELD, MISSING_FIELD, OUT_OF_RANGE
};

class ProvisioningWifiJson {
 public:
  static ProvisioningWifiParseResult parse(const char *json, size_t length,
                                           WifiCredentials &credentials);
};
