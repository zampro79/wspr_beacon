#pragma once

#include <stdint.h>

#include "BootModeController.h"

enum class OledDisplayTarget : uint8_t {
  PROVISIONING,
  PROVISIONING_ERROR,
  HARDWARE_CHECK,
  OPERATIONAL
};

class OledDisplayPolicy {
 public:
  static OledDisplayTarget select(SystemMode systemMode,
                                  bool hardwareCheckActive);
};
