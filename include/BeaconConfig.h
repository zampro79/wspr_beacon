#pragma once

#include <stdint.h>

#include "BandPlan.h"
#include "BuildProfile.h"
#include "WsprFrequencySelector.h"

struct BeaconConfig {
  char callsign[7] = WSPR_DEFAULT_CALLSIGN_LITERAL;
  char fixedLocator[5] = WSPR_DEFAULT_LOCATOR_LITERAL;
  uint8_t powerDbm = 10;
  bool useGpsLocator = true;
  uint8_t transmitIntervalMinutes = 4;
  uint32_t baseFrequencyHz = BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ;
  int16_t frequencyOffsetHz = 0;
  uint8_t preparationSeconds = 5;
  int32_t si5351Correction = 76000;
  uint32_t si5351ReferenceHz = 25000000UL;
  uint16_t passbandHz = 200;
  bool wifiEnabled = false;
  bool serialDiagnosticsEnabled = true;
  uint16_t serialDiagnosticsIntervalMs = 5000;
  WsprBandId band = BandPlan::DEFAULT_BAND;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
};
