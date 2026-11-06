#include <unity.h>

#include "HardwareCheckController.h"

namespace {
HardwareCheckInputs inputs(bool gps, DiagnosticState si5351) {
  HardwareCheckInputs value;
  value.gpsNmeaAvailable = gps;
  value.si5351State = si5351;
  return value;
}

void assertPhase(const HardwareCheckController &check,
                 HardwareCheckPhase expected) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                          static_cast<uint8_t>(check.snapshot().phase));
}
}  // namespace

void test_successful_checks_are_sequential_and_hold_for_three_seconds() {
  HardwareCheckController check;
  check.begin(5000UL, true);
  TEST_ASSERT_TRUE(check.active());
  TEST_ASSERT_FALSE(check.transmissionAllowed());
  assertPhase(check, HardwareCheckPhase::OLED);

  check.update(5249UL, inputs(true, DiagnosticState::STATE_OPERATIONAL));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::WAIT),
                          static_cast<uint8_t>(check.snapshot().oled));
  check.update(5250UL, inputs(true, DiagnosticState::STATE_OPERATIONAL));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::OK),
                          static_cast<uint8_t>(check.snapshot().oled));
  assertPhase(check, HardwareCheckPhase::GPS);

  check.update(5500UL, inputs(true, DiagnosticState::STATE_OPERATIONAL));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::OK),
                          static_cast<uint8_t>(check.snapshot().gps));
  assertPhase(check, HardwareCheckPhase::SI5351A);
  check.update(5750UL, inputs(true, DiagnosticState::STATE_DEGRADED));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::OK),
                          static_cast<uint8_t>(check.snapshot().si5351));
  assertPhase(check, HardwareCheckPhase::LED);
  TEST_ASSERT_TRUE(check.ledTestActive());

  check.update(6349UL, inputs(true, DiagnosticState::STATE_DEGRADED));
  TEST_ASSERT_TRUE(check.ledTestActive());
  check.update(6350UL, inputs(true, DiagnosticState::STATE_DEGRADED));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::OK),
                          static_cast<uint8_t>(check.snapshot().led));
  assertPhase(check, HardwareCheckPhase::HOLD);
  check.update(9349UL, inputs(true, DiagnosticState::STATE_DEGRADED));
  TEST_ASSERT_TRUE(check.active());
  check.update(9350UL, inputs(true, DiagnosticState::STATE_DEGRADED));
  TEST_ASSERT_TRUE(check.complete());
  TEST_ASSERT_TRUE(check.transmissionAllowed());
}

void test_absent_oled_and_timeouts_do_not_block_completion() {
  HardwareCheckController check;
  check.begin(0, false);
  check.update(250UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().oled));
  check.update(10249UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::WAIT),
                          static_cast<uint8_t>(check.snapshot().gps));
  check.update(10250UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().gps));
  check.update(11249UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::WAIT),
                          static_cast<uint8_t>(check.snapshot().si5351));
  check.update(11250UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().si5351));
  check.update(11850UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  check.update(14850UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_TRUE(check.complete());
}

void test_terminal_si5351_failure_is_reported_without_rf_probe() {
  HardwareCheckController check;
  check.begin(100UL, true);
  check.update(350UL, inputs(true, DiagnosticState::STATE_NOT_DETECTED));
  check.update(600UL, inputs(true, DiagnosticState::STATE_NOT_DETECTED));
  check.update(850UL, inputs(true, DiagnosticState::STATE_NOT_DETECTED));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().si5351));
  assertPhase(check, HardwareCheckPhase::LED);
}

void test_monotonic_overflow_preserves_durations() {
  HardwareCheckController check;
  check.begin(UINT32_MAX - 99UL, true);
  check.update(150UL, inputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::OK),
                          static_cast<uint8_t>(check.snapshot().oled));
  assertPhase(check, HardwareCheckPhase::GPS);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_successful_checks_are_sequential_and_hold_for_three_seconds);
  RUN_TEST(test_absent_oled_and_timeouts_do_not_block_completion);
  RUN_TEST(test_terminal_si5351_failure_is_reported_without_rf_probe);
  RUN_TEST(test_monotonic_overflow_preserves_durations);
  return UNITY_END();
}
