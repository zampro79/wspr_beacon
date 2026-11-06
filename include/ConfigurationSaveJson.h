#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigurationService.h"

class ConfigurationSaveJson {
 public:
  static bool serialize(const WsprSaveOutcome &outcome, uint32_t revision,
                        char *destination, size_t capacity, size_t &length);
};
