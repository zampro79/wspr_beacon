#pragma once

#include <stdint.h>

#include "BandPlan.h"
#include "BeaconSettings.h"

enum class ConfigField : uint8_t {
  CALLSIGN,
  LOCATOR,
  BAND,
  POWER_DBM,
  INTERVAL_MINUTES,
  BASE_FREQUENCY_HZ,
  FREQUENCY_OFFSET_HZ,
  SI5351_CORRECTION,
  POWER_SAVE
};
enum class ConfigValidation : uint8_t {
  VALID,
  INVALID_FORMAT,
  OUT_OF_RANGE,
  INVALID_COMBINATION
};
class ConfigValidator {
 public:
  static constexpr uint8_t MIN_CALLSIGN_LENGTH = 3;
  static constexpr uint8_t MAX_CALLSIGN_LENGTH = 6;
  static constexpr uint8_t LOCATOR_LENGTH = 4;
  static constexpr uint8_t MIN_INTERVAL_MINUTES = 2;
  static constexpr uint8_t MAX_INTERVAL_MINUTES = 60;
  static constexpr uint8_t INTERVAL_STEP_MINUTES = 2;
  static constexpr int16_t MIN_OFFSET_HZ = -100;
  static constexpr int16_t MAX_OFFSET_HZ = 100;
  static constexpr int32_t MIN_CORRECTION = -1000000L;
  static constexpr int32_t MAX_CORRECTION = 1000000L;

  static uint8_t allowedPowerCount();
  static bool allowedPowerAt(uint8_t index, uint8_t &value);

  static ConfigValidation validate(const BeaconSettings &settings);
  static ConfigValidation validateCallsign(const char *value);
  static ConfigValidation validateLocator(const char *value);
  static ConfigValidation validatePower(uint8_t value);
  static ConfigValidation validateInterval(uint8_t value);
  static ConfigValidation validateBaseFrequency(uint32_t value);
  static ConfigValidation validateBaseFrequency(WsprBandId band,
                                                uint32_t value);
  static ConfigValidation validateOffset(int32_t value);
  static ConfigValidation validateCorrection(int32_t value);
  static ConfigValidation validateBand(WsprBandId band);
  static ConfigValidation validateFrequencyMode(WsprFrequencyMode mode);
  static ConfigValidation validateToneSet(uint32_t baseFrequencyHz,
                                          int32_t offsetHz);
  static ConfigValidation validateToneSet(WsprBandId band,
                                          uint32_t baseFrequencyHz,
                                          int32_t offsetHz);
};
