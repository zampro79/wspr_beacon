#include <unity.h>

#include "TimeAuthorityLedController.h"

namespace {
TimeAuthorityLedOutput sample(
    TimeAuthorityLedController &led, uint32_t now,
    WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE,
    bool startup = false, bool hardwareTest = false,
    bool timingCritical = false) {
  TimeAuthorityLedInputs inputs;
  inputs.authorization = authorization;
  inputs.startupActive = startup;
  inputs.hardwareTestActive = hardwareTest;
  inputs.timingCritical = timingCritical;
  return led.update(now, inputs);
}
}  // namespace

void test_no_qualified_time_is_off() {
  TimeAuthorityLedController led;
  led.begin(100UL);
  const TimeAuthorityLedOutput output = sample(led, 100UL);
  TEST_ASSERT_FALSE(output.enabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TimeAuthorityLedMode::OFF),
                          static_cast<uint8_t>(output.mode));
}

void test_ntp_authority_blinks_slowly() {
  TimeAuthorityLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 100UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_TRUE(sample(led, 1099UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_FALSE(sample(led, 1100UL,
                           WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_TRUE(sample(led, 2100UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
}

void test_gps_authority_is_steady_on() {
  TimeAuthorityLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 10UL,
                          WsprTimeAuthorization::GPS_PRECISE).enabled);
  TEST_ASSERT_TRUE(sample(led, 60000UL,
                          WsprTimeAuthorization::GPS_PRECISE).enabled);
}

void test_source_transitions_restart_the_selected_pattern() {
  TimeAuthorityLedController led;
  led.begin(0);
  TEST_ASSERT_FALSE(sample(led, 10UL).enabled);
  TEST_ASSERT_TRUE(sample(led, 20UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_TRUE(sample(led, 1500UL,
                          WsprTimeAuthorization::GPS_PRECISE).enabled);
  TEST_ASSERT_FALSE(sample(led, 1600UL).enabled);
}

void test_preparing_and_tx_freeze_output_and_source() {
  TimeAuthorityLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 100UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  const TimeAuthorityLedOutput preparing = sample(
      led, 1200UL, WsprTimeAuthorization::NONE, false, false, true);
  TEST_ASSERT_TRUE(preparing.enabled);
  TEST_ASSERT_FALSE(preparing.changed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TimeAuthorityLedMode::NTP),
                          static_cast<uint8_t>(preparing.mode));
  const TimeAuthorityLedOutput transmitting = sample(
      led, 60000UL, WsprTimeAuthorization::GPS_PRECISE, false, false, true);
  TEST_ASSERT_TRUE(transmitting.enabled);
  TEST_ASSERT_FALSE(transmitting.changed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TimeAuthorityLedMode::NTP),
                          static_cast<uint8_t>(transmitting.mode));
  TEST_ASSERT_TRUE(sample(led, 60001UL,
                          WsprTimeAuthorization::GPS_PRECISE).enabled);
}

void test_startup_is_off_and_hardware_check_flashes_three_times() {
  TimeAuthorityLedController led;
  led.begin(0);
  TEST_ASSERT_FALSE(sample(led, 4999UL,
                           WsprTimeAuthorization::GPS_PRECISE, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5000UL, WsprTimeAuthorization::GPS_PRECISE,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5100UL, WsprTimeAuthorization::GPS_PRECISE,
                           false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5200UL, WsprTimeAuthorization::GPS_PRECISE,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5300UL, WsprTimeAuthorization::GPS_PRECISE,
                           false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5400UL, WsprTimeAuthorization::GPS_PRECISE,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5500UL, WsprTimeAuthorization::GPS_PRECISE,
                           false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5600UL,
                          WsprTimeAuthorization::GPS_PRECISE).enabled);
}

void test_blink_timing_handles_millis_overflow() {
  TimeAuthorityLedController led;
  led.begin(UINT32_MAX - 500UL);
  TEST_ASSERT_TRUE(sample(led, UINT32_MAX - 500UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_TRUE(sample(led, 498UL,
                          WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
  TEST_ASSERT_FALSE(sample(led, 499UL,
                           WsprTimeAuthorization::NTP_EXPERIMENTAL).enabled);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_qualified_time_is_off);
  RUN_TEST(test_ntp_authority_blinks_slowly);
  RUN_TEST(test_gps_authority_is_steady_on);
  RUN_TEST(test_source_transitions_restart_the_selected_pattern);
  RUN_TEST(test_preparing_and_tx_freeze_output_and_source);
  RUN_TEST(test_startup_is_off_and_hardware_check_flashes_three_times);
  RUN_TEST(test_blink_timing_handles_millis_overflow);
  return UNITY_END();
}
