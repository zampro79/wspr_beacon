#pragma once

#include <stddef.h>

#include "ConfigurationService.h"

class ConfigurationJson {
 public:
  // Shares the existing bounded HTTP JSON buffer with /api/status.
  static constexpr size_t MAX_RESPONSE_BYTES = 2048;

  static bool serialize(const ConfigurationSnapshot &snapshot,
                        char *destination, size_t capacity, size_t &length);
};
