#pragma once

#include <stdint.h>

#include "BandPlan.h"

enum class WsprFrequencyMode : uint8_t { FIXED, RANDOM };

class WsprFrequencySelector {
 public:
  static constexpr int16_t RANDOM_MIN_OFFSET_HZ = -90;
  static constexpr int16_t RANDOM_MAX_OFFSET_HZ = 89;
  static constexpr uint16_t RANDOM_VALUE_COUNT = 180;

  void seedOnce(uint32_t seed);
  uint32_t select(WsprFrequencyMode mode, WsprBandId band,
                  uint32_t fixedFrequencyHz);
  bool seeded() const { return seeded_; }

 private:
  static constexpr uint32_t FALLBACK_SEED = 0x6D2B79F5UL;
  uint32_t state_ = FALLBACK_SEED;
  bool seeded_ = false;

  uint32_t next();
  uint32_t uniform(uint32_t range);
};

const char *wsprFrequencyModeLabel(WsprFrequencyMode mode);
