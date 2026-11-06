#include "WsprFrequencySelector.h"

void WsprFrequencySelector::seedOnce(uint32_t seed) {
  if (seeded_) return;
  state_ = seed == 0 ? FALLBACK_SEED : seed;
  seeded_ = true;
}

uint32_t WsprFrequencySelector::next() {
  if (!seeded_) seedOnce(FALLBACK_SEED);
  uint32_t value = state_;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  state_ = value;
  return value;
}

uint32_t WsprFrequencySelector::uniform(uint32_t range) {
  if (range < 2U) return 0;
  const uint32_t threshold = static_cast<uint32_t>(-range) % range;
  uint32_t value = 0;
  do {
    value = next();
  } while (value < threshold);
  return value % range;
}

uint32_t WsprFrequencySelector::select(WsprFrequencyMode mode,
                                       WsprBandId band,
                                       uint32_t fixedFrequencyHz) {
  if (mode != WsprFrequencyMode::RANDOM || !BandPlan::txEnabled(band)) {
    return fixedFrequencyHz;
  }
  const BandPlanEntry *entry = BandPlan::find(band);
  if (entry == nullptr) return fixedFrequencyHz;
  const uint32_t selected = static_cast<uint32_t>(
      static_cast<int64_t>(entry->nominalToneZeroHz) +
      RANDOM_MIN_OFFSET_HZ + uniform(RANDOM_VALUE_COUNT));
  return BandPlan::validateToneSet(band, selected, 0) ==
                 BandFrequencyValidation::VALID
             ? selected
             : fixedFrequencyHz;
}

const char *wsprFrequencyModeLabel(WsprFrequencyMode mode) {
  return mode == WsprFrequencyMode::RANDOM ? "RANDOM" : "FIXED";
}
