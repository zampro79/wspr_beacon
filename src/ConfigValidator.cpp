#include "ConfigValidator.h"

#include <string.h>

namespace {
const uint8_t ALLOWED_POWERS[] = {
    0, 3, 7, 10, 13, 17, 20, 23, 27, 30,
    33, 37, 40, 43, 47, 50, 53, 57, 60};
constexpr uint8_t ALLOWED_POWER_COUNT =
    sizeof(ALLOWED_POWERS) / sizeof(ALLOWED_POWERS[0]);
}  // namespace

uint8_t ConfigValidator::allowedPowerCount() { return ALLOWED_POWER_COUNT; }

bool ConfigValidator::allowedPowerAt(uint8_t index, uint8_t &value) {
  if (index >= ALLOWED_POWER_COUNT) return false;
  value = ALLOWED_POWERS[index];
  return true;
}

ConfigValidation ConfigValidator::validateCallsign(const char *value) {
  if (value == nullptr) return ConfigValidation::INVALID_FORMAT;
  const size_t length = strlen(value);
  if (length < MIN_CALLSIGN_LENGTH || length > MAX_CALLSIGN_LENGTH) {
    return ConfigValidation::INVALID_FORMAT;
  }
  for (size_t index = 0; index < length; ++index) {
    if (!((value[index] >= 'A' && value[index] <= 'Z') ||
          (value[index] >= '0' && value[index] <= '9'))) {
      return ConfigValidation::INVALID_FORMAT;
    }
  }
  const bool digitAtSecond = value[1] >= '0' && value[1] <= '9';
  const bool digitAtThird =
      length > 2 && value[2] >= '0' && value[2] <= '9';
  const size_t digitIndex = digitAtSecond ? 1U : 2U;
  if ((!digitAtSecond && !digitAtThird) ||
      value[0] < 'A' || value[0] > 'Z') {
    return ConfigValidation::INVALID_FORMAT;
  }
  for (size_t index = digitIndex + 1; index < length; ++index) {
    if (value[index] < 'A' || value[index] > 'Z') {
      return ConfigValidation::INVALID_FORMAT;
    }
  }
  return ConfigValidation::VALID;
}
ConfigValidation ConfigValidator::validateLocator(const char *value) {
  if (value == nullptr || strlen(value) != LOCATOR_LENGTH) {
    return ConfigValidation::INVALID_FORMAT;
  }
  if (value[0] < 'A' || value[0] > 'R' || value[1] < 'A' || value[1] > 'R' ||
      value[2] < '0' || value[2] > '9' || value[3] < '0' || value[3] > '9') {
    return ConfigValidation::INVALID_FORMAT;
  }
  return ConfigValidation::VALID;
}
ConfigValidation ConfigValidator::validatePower(uint8_t value) {
  for (uint8_t index = 0; index < allowedPowerCount(); ++index) {
    uint8_t candidate = 0;
    if (allowedPowerAt(index, candidate) && value == candidate) {
      return ConfigValidation::VALID;
    }
  }
  return ConfigValidation::OUT_OF_RANGE;
}

ConfigValidation ConfigValidator::validateInterval(uint8_t value) {
  return value >= MIN_INTERVAL_MINUTES && value <= MAX_INTERVAL_MINUTES &&
                 (value % INTERVAL_STEP_MINUTES) == 0
             ? ConfigValidation::VALID
             : ConfigValidation::OUT_OF_RANGE;
}

ConfigValidation ConfigValidator::validateBaseFrequency(uint32_t value) {
  return validateBaseFrequency(BandPlan::DEFAULT_BAND, value);
}

ConfigValidation ConfigValidator::validateBaseFrequency(WsprBandId band,
                                                         uint32_t value) {
  return BandPlan::validateFrequency(band, value) ==
                 BandFrequencyValidation::VALID
             ? ConfigValidation::VALID
             : ConfigValidation::OUT_OF_RANGE;
}

ConfigValidation ConfigValidator::validateOffset(int32_t value) {
  return value >= MIN_OFFSET_HZ && value <= MAX_OFFSET_HZ
             ? ConfigValidation::VALID
             : ConfigValidation::OUT_OF_RANGE;
}

ConfigValidation ConfigValidator::validateCorrection(int32_t value) {
  return value >= MIN_CORRECTION && value <= MAX_CORRECTION
             ? ConfigValidation::VALID
             : ConfigValidation::OUT_OF_RANGE;
}

ConfigValidation ConfigValidator::validateBand(WsprBandId band) {
  return BandPlan::find(band) != nullptr && BandPlan::txEnabled(band)
             ? ConfigValidation::VALID
             : ConfigValidation::INVALID_COMBINATION;
}

ConfigValidation ConfigValidator::validateFrequencyMode(
    WsprFrequencyMode mode) {
  return mode == WsprFrequencyMode::FIXED ||
                 mode == WsprFrequencyMode::RANDOM
             ? ConfigValidation::VALID
             : ConfigValidation::INVALID_COMBINATION;
}

ConfigValidation ConfigValidator::validateToneSet(uint32_t baseFrequencyHz,
                                                  int32_t offsetHz) {
  return validateToneSet(BandPlan::DEFAULT_BAND, baseFrequencyHz, offsetHz);
}

ConfigValidation ConfigValidator::validateToneSet(
    WsprBandId band, uint32_t baseFrequencyHz, int32_t offsetHz) {
  const bool txAllowed = BandPlan::txEnabled(band);
  const bool tonesValid =
      BandPlan::validateToneSet(band, baseFrequencyHz, offsetHz) ==
      BandFrequencyValidation::VALID;
  return txAllowed && tonesValid
             ? ConfigValidation::VALID
             : ConfigValidation::INVALID_COMBINATION;
}

ConfigValidation ConfigValidator::validate(const BeaconSettings &settings) {
  ConfigValidation result = validateCallsign(settings.callsign);
  if (result != ConfigValidation::VALID) return result;
  result = validateLocator(settings.fixedLocator);
  if (result != ConfigValidation::VALID) return result;
  result = validatePower(settings.powerDbm);
  if (result != ConfigValidation::VALID) return result;
  result = validateInterval(settings.transmitIntervalMinutes);
  if (result != ConfigValidation::VALID) return result;
  result = validateBand(settings.band);
  if (result != ConfigValidation::VALID) return result;
  result = validateFrequencyMode(settings.frequencyMode);
  if (result != ConfigValidation::VALID) return result;
  result = BandPlan::validateFrequency(settings.band,
                                       settings.baseFrequencyHz) ==
                   BandFrequencyValidation::VALID
               ? ConfigValidation::VALID
               : ConfigValidation::OUT_OF_RANGE;
  if (result != ConfigValidation::VALID) return result;
  result = validateOffset(settings.frequencyOffsetHz);
  if (result != ConfigValidation::VALID) return result;
  result = validateCorrection(settings.si5351Correction);
  if (result != ConfigValidation::VALID) return result;
  const bool tonesValid =
      BandPlan::validateToneSet(settings.band, settings.baseFrequencyHz,
                                settings.frequencyOffsetHz) ==
      BandFrequencyValidation::VALID;
  return tonesValid ? ConfigValidation::VALID
                    : ConfigValidation::INVALID_COMBINATION;
}
