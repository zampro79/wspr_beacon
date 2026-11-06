#include <unity.h>

#include "BootModeController.h"
#include "HardwareCheckController.h"
#include "OledDisplayPolicy.h"

namespace {
void assertTarget(SystemMode mode, bool hardwareCheckActive,
                  OledDisplayTarget expected) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(expected),
      static_cast<uint8_t>(
          OledDisplayPolicy::select(mode, hardwareCheckActive)));
}

HardwareCheckInputs hardwareInputs(bool gps, DiagnosticState si5351) {
  HardwareCheckInputs inputs;
  inputs.gpsNmeaAvailable = gps;
  inputs.si5351State = si5351;
  return inputs;
}
}  // namespace

void test_immediate_provisioning_has_priority_over_hardware_check() {
  BootModeController boot;
  boot.begin(false, 0);
  TEST_ASSERT_TRUE(boot.enterApRequested());
  boot.apStartResult(true);
  assertTarget(boot.mode(), true, OledDisplayTarget::PROVISIONING);
  TEST_ASSERT_FALSE(boot.rfOperationAllowed());
}

void test_provisioning_remains_visible_while_hardware_check_completes() {
  HardwareCheckController check;
  check.begin(5000, true);
  const SystemMode mode = SystemMode::PROVISIONING_AP;
  assertTarget(mode, check.active(), OledDisplayTarget::PROVISIONING);

  check.update(5250, hardwareInputs(true, DiagnosticState::STATE_OPERATIONAL));
  check.update(5500, hardwareInputs(true, DiagnosticState::STATE_OPERATIONAL));
  check.update(5750, hardwareInputs(true, DiagnosticState::STATE_OPERATIONAL));
  check.update(6350, hardwareInputs(true, DiagnosticState::STATE_OPERATIONAL));
  check.update(9350, hardwareInputs(true, DiagnosticState::STATE_OPERATIONAL));

  TEST_ASSERT_TRUE(check.complete());
  assertTarget(mode, check.active(), OledDisplayTarget::PROVISIONING);
}

void test_gps_timeout_does_not_displace_provisioning() {
  HardwareCheckController check;
  check.begin(5000, true);
  check.update(5250, hardwareInputs(false, DiagnosticState::STATE_UNKNOWN));
  check.update(15250, hardwareInputs(false, DiagnosticState::STATE_UNKNOWN));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().gps));
  assertTarget(SystemMode::PROVISIONING_AP, check.active(),
               OledDisplayTarget::PROVISIONING);
}

void test_si5351_failure_does_not_displace_provisioning() {
  HardwareCheckController check;
  check.begin(0, true);
  check.update(250, hardwareInputs(true, DiagnosticState::STATE_NOT_DETECTED));
  check.update(500, hardwareInputs(true, DiagnosticState::STATE_NOT_DETECTED));
  check.update(750, hardwareInputs(true, DiagnosticState::STATE_NOT_DETECTED));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HardwareCheckResult::FAIL),
                          static_cast<uint8_t>(check.snapshot().si5351));
  assertTarget(SystemMode::PROVISIONING_AP, check.active(),
               OledDisplayTarget::PROVISIONING);
}

void test_sta_timeout_selects_provisioning() {
  BootModeController boot;
  boot.begin(true, 1000);
  boot.update(false, false,
              1000 + BootModeController::INITIAL_STA_TIMEOUT_MS);
  TEST_ASSERT_TRUE(boot.enterApRequested());
  boot.apStartResult(true);
  assertTarget(boot.mode(), true, OledDisplayTarget::PROVISIONING);
}

void test_ap_start_failure_selects_error_not_stale_credentials() {
  BootModeController boot;
  boot.begin(false, 0);
  boot.apStartResult(false);
  assertTarget(boot.mode(), true, OledDisplayTarget::PROVISIONING_ERROR);
  TEST_ASSERT_FALSE(boot.rfOperationAllowed());
}

void test_ap_operational_preserves_existing_hardware_check_priority() {
  BootModeController boot;
  boot.begin(false, 0);
  boot.apStartResult(true);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ApOperationalRequestResult::ACCEPTED),
      static_cast<uint8_t>(boot.requestApOperational()));
  TEST_ASSERT_TRUE(boot.completeApOperationalTransition());
  assertTarget(boot.mode(), true, OledDisplayTarget::HARDWARE_CHECK);
  assertTarget(boot.mode(), false, OledDisplayTarget::OPERATIONAL);
}

void test_normal_sta_preserves_existing_screen_sequence() {
  BootModeController boot;
  boot.begin(true, 0);
  boot.update(true, false, 1);
  assertTarget(boot.mode(), true, OledDisplayTarget::HARDWARE_CHECK);
  assertTarget(boot.mode(), false, OledDisplayTarget::OPERATIONAL);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_immediate_provisioning_has_priority_over_hardware_check);
  RUN_TEST(test_provisioning_remains_visible_while_hardware_check_completes);
  RUN_TEST(test_gps_timeout_does_not_displace_provisioning);
  RUN_TEST(test_si5351_failure_does_not_displace_provisioning);
  RUN_TEST(test_sta_timeout_selects_provisioning);
  RUN_TEST(test_ap_start_failure_selects_error_not_stale_credentials);
  RUN_TEST(test_ap_operational_preserves_existing_hardware_check_priority);
  RUN_TEST(test_normal_sta_preserves_existing_screen_sequence);
  return UNITY_END();
}
