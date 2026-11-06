#pragma once

#include <stdint.h>

#include "BandPlan.h"
#include "WsprFrequencySelector.h"
#include "WsprScheduler.h"

struct WsprMessageSnapshot {
  char callsign[7] = {};
  char locator[5] = {};
  uint8_t powerDbm = 0;
};
struct TxPreparedSnapshot {
  WsprMessageSnapshot message;
  uint32_t frequencyHz = 0;
  uint32_t slotUtcMillis = 0;
  WsprBandId band = BandPlan::DEFAULT_BAND;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  bool valid = false;
};
