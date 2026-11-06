#include "TimeAuthorityLedController.h"

void TimeAuthorityLedController::begin(uint32_t monotonicMillis) {
  mode_ = TimeAuthorityLedMode::OFF;
  modeStartedMillis_ = monotonicMillis;
  outputEnabled_ = false;
  initialized_ = true;
}

void TimeAuthorityLedController::setMode(TimeAuthorityLedMode mode,
                                         uint32_t monotonicMillis) {
  if (mode_ == mode) return;
  mode_ = mode;
  modeStartedMillis_ = monotonicMillis;
}

TimeAuthorityLedOutput TimeAuthorityLedController::output(bool changed) const {
  TimeAuthorityLedOutput result;
  result.enabled = outputEnabled_;
  result.changed = changed;
  result.mode = mode_;
  return result;
}

TimeAuthorityLedOutput TimeAuthorityLedController::update(
    uint32_t monotonicMillis, const TimeAuthorityLedInputs &inputs) {
  if (!initialized_) begin(monotonicMillis);

  if (inputs.startupActive) {
    setMode(TimeAuthorityLedMode::OFF, monotonicMillis);
  } else if (inputs.hardwareTestActive) {
    setMode(TimeAuthorityLedMode::HARDWARE_TEST, monotonicMillis);
  } else if (inputs.timingCritical) {
    return output(false);
  } else if (inputs.authorization == WsprTimeAuthorization::GPS_PRECISE) {
    setMode(TimeAuthorityLedMode::GPS, monotonicMillis);
  } else if (inputs.authorization ==
             WsprTimeAuthorization::NTP_EXPERIMENTAL) {
    setMode(TimeAuthorityLedMode::NTP, monotonicMillis);
  } else {
    setMode(TimeAuthorityLedMode::OFF, monotonicMillis);
  }

  const bool enabled = pattern(
      mode_, static_cast<uint32_t>(monotonicMillis - modeStartedMillis_));
  const bool changed = enabled != outputEnabled_;
  outputEnabled_ = enabled;
  return output(changed);
}

bool TimeAuthorityLedController::pattern(TimeAuthorityLedMode mode,
                                         uint32_t elapsedMillis) {
  switch (mode) {
    case TimeAuthorityLedMode::OFF:
      return false;
    case TimeAuthorityLedMode::HARDWARE_TEST:
      return elapsedMillis < 600UL && (elapsedMillis % 200UL) < 100UL;
    case TimeAuthorityLedMode::NTP:
      return (elapsedMillis % 2000UL) < 1000UL;
    case TimeAuthorityLedMode::GPS:
      return true;
  }
  return false;
}
