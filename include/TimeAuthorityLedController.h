#pragma once

#include <stdint.h>

#include "WsprScheduler.h"

enum class TimeAuthorityLedMode : uint8_t {
  OFF,
  HARDWARE_TEST,
  NTP,
  GPS
};

struct TimeAuthorityLedInputs {
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  bool startupActive = false;
  bool hardwareTestActive = false;
  bool timingCritical = false;
};

struct TimeAuthorityLedOutput {
  bool enabled = false;
  bool changed = false;
  TimeAuthorityLedMode mode = TimeAuthorityLedMode::OFF;
};

class TimeAuthorityLedController {
 public:
  void begin(uint32_t monotonicMillis);
  TimeAuthorityLedOutput update(uint32_t monotonicMillis,
                                const TimeAuthorityLedInputs &inputs);

 private:
  TimeAuthorityLedMode mode_ = TimeAuthorityLedMode::OFF;
  uint32_t modeStartedMillis_ = 0;
  bool outputEnabled_ = false;
  bool initialized_ = false;

  void setMode(TimeAuthorityLedMode mode, uint32_t monotonicMillis);
  static bool pattern(TimeAuthorityLedMode mode, uint32_t elapsedMillis);
  TimeAuthorityLedOutput output(bool changed) const;
};
