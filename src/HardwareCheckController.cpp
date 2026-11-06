#include "HardwareCheckController.h"

void HardwareCheckController::begin(uint32_t nowMillis,
                                    bool oledInitialized) {
  snapshot_ = HardwareCheckSnapshot();
  snapshot_.phase = HardwareCheckPhase::OLED;
  snapshot_.revision = 1;
  phaseStartedMillis_ = nowMillis;
  oledInitialized_ = oledInitialized;
}

void HardwareCheckController::setResult(HardwareCheckResult &target,
                                        HardwareCheckResult result) {
  if (target == result) return;
  target = result;
  ++snapshot_.revision;
}

void HardwareCheckController::advance(HardwareCheckPhase phase,
                                      uint32_t nowMillis) {
  snapshot_.phase = phase;
  phaseStartedMillis_ = nowMillis;
}

void HardwareCheckController::update(
    uint32_t nowMillis, const HardwareCheckInputs &inputs) {
  const uint32_t elapsed = nowMillis - phaseStartedMillis_;
  switch (snapshot_.phase) {
    case HardwareCheckPhase::OLED:
      if (elapsed >= MIN_CHECK_MS) {
        setResult(snapshot_.oled, oledInitialized_ ? HardwareCheckResult::OK
                                                  : HardwareCheckResult::FAIL);
        advance(HardwareCheckPhase::GPS, nowMillis);
      }
      break;
    case HardwareCheckPhase::GPS:
      if (inputs.gpsNmeaAvailable && elapsed >= MIN_CHECK_MS) {
        setResult(snapshot_.gps, HardwareCheckResult::OK);
        advance(HardwareCheckPhase::SI5351A, nowMillis);
      } else if (elapsed >= GPS_TIMEOUT_MS) {
        setResult(snapshot_.gps, HardwareCheckResult::FAIL);
        advance(HardwareCheckPhase::SI5351A, nowMillis);
      }
      break;
    case HardwareCheckPhase::SI5351A:
      if (elapsed >= MIN_CHECK_MS &&
          (inputs.si5351State == DiagnosticState::STATE_OPERATIONAL ||
           inputs.si5351State == DiagnosticState::STATE_DEGRADED)) {
        setResult(snapshot_.si5351, HardwareCheckResult::OK);
        advance(HardwareCheckPhase::LED, nowMillis);
      } else if (elapsed >= MIN_CHECK_MS &&
                 (inputs.si5351State == DiagnosticState::STATE_NOT_DETECTED ||
                  inputs.si5351State == DiagnosticState::STATE_ERROR ||
                  inputs.si5351State == DiagnosticState::STATE_DISABLED)) {
        setResult(snapshot_.si5351, HardwareCheckResult::FAIL);
        advance(HardwareCheckPhase::LED, nowMillis);
      } else if (elapsed >= SI5351_TIMEOUT_MS) {
        setResult(snapshot_.si5351, HardwareCheckResult::FAIL);
        advance(HardwareCheckPhase::LED, nowMillis);
      }
      break;
    case HardwareCheckPhase::LED:
      if (elapsed >= LED_TEST_MS) {
        setResult(snapshot_.led, HardwareCheckResult::OK);
        advance(HardwareCheckPhase::HOLD, nowMillis);
      }
      break;
    case HardwareCheckPhase::HOLD:
      if (elapsed >= FINAL_HOLD_MS) advance(HardwareCheckPhase::COMPLETE, nowMillis);
      break;
    case HardwareCheckPhase::NOT_STARTED:
    case HardwareCheckPhase::COMPLETE:
      break;
  }
}
