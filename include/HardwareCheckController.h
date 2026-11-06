#pragma once

#include <stdint.h>

#include "DiagnosticRegistry.h"

enum class HardwareCheckResult : uint8_t { WAIT, OK, FAIL };
enum class HardwareCheckPhase : uint8_t {
  NOT_STARTED,
  OLED,
  GPS,
  SI5351A,
  LED,
  HOLD,
  COMPLETE
};

struct HardwareCheckInputs {
  bool gpsNmeaAvailable = false;
  DiagnosticState si5351State = DiagnosticState::STATE_UNKNOWN;
};

struct HardwareCheckSnapshot {
  HardwareCheckResult oled = HardwareCheckResult::WAIT;
  HardwareCheckResult gps = HardwareCheckResult::WAIT;
  HardwareCheckResult si5351 = HardwareCheckResult::WAIT;
  HardwareCheckResult led = HardwareCheckResult::WAIT;
  HardwareCheckPhase phase = HardwareCheckPhase::NOT_STARTED;
  uint8_t revision = 0;
};

class HardwareCheckController {
 public:
  static constexpr uint32_t MIN_CHECK_MS = 250UL;
  static constexpr uint32_t GPS_TIMEOUT_MS = 10000UL;
  static constexpr uint32_t SI5351_TIMEOUT_MS = 1000UL;
  static constexpr uint32_t LED_TEST_MS = 600UL;
  static constexpr uint32_t FINAL_HOLD_MS = 3000UL;

  void begin(uint32_t nowMillis, bool oledInitialized);
  void update(uint32_t nowMillis, const HardwareCheckInputs &inputs);
  bool started() const { return snapshot_.phase != HardwareCheckPhase::NOT_STARTED; }
  bool active() const { return started() && !complete(); }
  bool complete() const { return snapshot_.phase == HardwareCheckPhase::COMPLETE; }
  bool transmissionAllowed() const { return complete(); }
  bool ledTestActive() const { return snapshot_.phase == HardwareCheckPhase::LED; }
  const HardwareCheckSnapshot &snapshot() const { return snapshot_; }

 private:
  HardwareCheckSnapshot snapshot_;
  uint32_t phaseStartedMillis_ = 0;
  bool oledInitialized_ = false;

  void advance(HardwareCheckPhase phase, uint32_t nowMillis);
  void setResult(HardwareCheckResult &target, HardwareCheckResult result);
};
