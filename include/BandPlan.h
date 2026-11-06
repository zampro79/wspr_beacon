#pragma once

#include <stdint.h>

enum class WsprBandId : uint8_t {
  BAND_40M = 0,
  BAND_30M = 1,
  BAND_20M = 2,
  BAND_17M = 3,
  BAND_15M = 4,
  BAND_12M = 5,
  BAND_10M = 6
};

struct BandPlanEntry {
  WsprBandId id;
  const char *name;
  uint32_t nominalToneZeroHz;
  uint32_t lowerLimitHz;
  uint32_t upperLimitHz;
  bool txEnabled;
};

enum class BandFrequencyValidation : uint8_t {
  VALID,
  UNKNOWN_BAND,
  OUTSIDE_WINDOW
};

class BandPlan {
 public:
  static constexpr uint8_t BAND_COUNT = 7;
  static constexpr WsprBandId DEFAULT_BAND = WsprBandId::BAND_20M;
  static constexpr uint32_t DEFAULT_NOMINAL_TONE_ZERO_HZ = 14097100UL;
  static constexpr uint16_t TONE_SPACING_CENTIHZ = 146;
  static constexpr uint8_t HIGHEST_TONE = 3;

  static uint8_t size() { return BAND_COUNT; }
  static const BandPlanEntry *at(uint8_t index);
  static const BandPlanEntry *find(WsprBandId id);
  static bool txEnabled(WsprBandId id);
  static BandFrequencyValidation validateFrequency(WsprBandId id,
                                                    uint32_t frequencyHz);
  static BandFrequencyValidation validateToneSet(WsprBandId id,
                                                  uint32_t baseFrequencyHz,
                                                  int32_t offsetHz);
};
