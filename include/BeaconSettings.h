#pragma once

#include <stdint.h>

#include "BandPlan.h"
#include "BuildProfile.h"
#include "WsprFrequencySelector.h"

struct BeaconSettings {
  char callsign[7] = WSPR_DEFAULT_CALLSIGN_LITERAL;
  char fixedLocator[5] = WSPR_DEFAULT_LOCATOR_LITERAL;
  uint32_t baseFrequencyHz = 14097100UL;
  int16_t frequencyOffsetHz = 0;
  int32_t si5351Correction = 76000;
  uint8_t powerDbm = 10;
  uint8_t transmitIntervalMinutes = 4;
  WsprBandId band = BandPlan::DEFAULT_BAND;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  bool powerSave = false;
};
