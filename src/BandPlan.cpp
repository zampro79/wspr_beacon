#include "BandPlan.h"

namespace {
const BandPlanEntry BANDS[BandPlan::BAND_COUNT] = {
    {WsprBandId::BAND_40M, "40m", 7040100UL, 7040000UL, 7040200UL, true},
    {WsprBandId::BAND_30M, "30m", 10140200UL, 10140100UL, 10140300UL, true},
    {WsprBandId::BAND_20M, "20m", BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ,
     14097000UL, 14097200UL, true},
    {WsprBandId::BAND_17M, "17m", 18106100UL, 18106000UL, 18106200UL, true},
    {WsprBandId::BAND_15M, "15m", 21096100UL, 21096000UL, 21096200UL, true},
    {WsprBandId::BAND_12M, "12m", 24926100UL, 24926000UL, 24926200UL, true},
    {WsprBandId::BAND_10M, "10m", 28126100UL, 28126000UL, 28126200UL, true},
};
}  // namespace

const BandPlanEntry *BandPlan::at(uint8_t index) {
  return index < BAND_COUNT ? &BANDS[index] : nullptr;
}

const BandPlanEntry *BandPlan::find(WsprBandId id) {
  for (uint8_t index = 0; index < BAND_COUNT; ++index) {
    if (BANDS[index].id == id) return &BANDS[index];
  }
  return nullptr;
}

bool BandPlan::txEnabled(WsprBandId id) {
  const BandPlanEntry *band = find(id);
  return band != nullptr && band->txEnabled;
}

BandFrequencyValidation BandPlan::validateFrequency(WsprBandId id,
                                                     uint32_t frequencyHz) {
  const BandPlanEntry *band = find(id);
  if (band == nullptr) return BandFrequencyValidation::UNKNOWN_BAND;
  return frequencyHz >= band->lowerLimitHz && frequencyHz <= band->upperLimitHz
             ? BandFrequencyValidation::VALID
             : BandFrequencyValidation::OUTSIDE_WINDOW;
}

BandFrequencyValidation BandPlan::validateToneSet(
    WsprBandId id, uint32_t baseFrequencyHz, int32_t offsetHz) {
  const BandPlanEntry *band = find(id);
  if (band == nullptr) return BandFrequencyValidation::UNKNOWN_BAND;

  const int64_t tone0Centihz =
      (static_cast<int64_t>(baseFrequencyHz) + offsetHz) * 100LL;
  const int64_t tone3Centihz =
      tone0Centihz + static_cast<int64_t>(HIGHEST_TONE) *
                            TONE_SPACING_CENTIHZ;
  const int64_t lowerCentihz =
      static_cast<int64_t>(band->lowerLimitHz) * 100LL;
  const int64_t upperCentihz =
      static_cast<int64_t>(band->upperLimitHz) * 100LL;
  return tone0Centihz >= lowerCentihz && tone3Centihz <= upperCentihz
             ? BandFrequencyValidation::VALID
             : BandFrequencyValidation::OUTSIDE_WINDOW;
}
