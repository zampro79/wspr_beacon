#pragma once

#include <stdint.h>

#include "BeaconSettings.h"

enum class OperatingIdentityState : uint8_t {
  OPERATING_IDENTITY_UNCONFIGURED,
  CONFIGURED
};

inline OperatingIdentityState operatingIdentityState(
    const BeaconSettings &settings) {
  return settings.callsign[0] == '\0'
      ? OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED
      : OperatingIdentityState::CONFIGURED;
}

inline bool operatingIdentityConfigured(const BeaconSettings &settings) {
  return operatingIdentityState(settings) ==
         OperatingIdentityState::CONFIGURED;
}
