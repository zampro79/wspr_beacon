#include <unity.h>

#include "StatusLedController.h"
#include "StatusLedPolarity.h"

namespace {
StatusLedOutput sample(StatusLedController &led, uint32_t now,
                       bool rfError = false, bool tx = false,
                       bool preparing = false, bool wifi = false,
                       bool ready = false, bool startup = false,
                       bool hardwareTest = false) {
  StatusLedInputs inputs;
  inputs.startupActive = startup;
  inputs.hardwareTestActive = hardwareTest;
  inputs.rfError = rfError;
  inputs.transmitting = tx;
  inputs.preparing = preparing;
  inputs.wifiConfiguring = wifi;
  inputs.ready = ready;
  return led.update(now, inputs);
}

void test_hardware_test_runs_three_flashes_then_releases_cleanly() {
  StatusLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 5000UL, false, false, false, false, false,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5100UL, false, false, false, false, false,
                           false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5200UL, false, false, false, false, false,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5300UL, false, false, false, false, false,
                           false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5400UL, false, false, false, false, false,
                          false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5500UL, false, false, false, false, false,
                           false, true).enabled);
  const StatusLedOutput released = sample(led, 5600UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::WAITING_SYNC),
                          static_cast<uint8_t>(released.mode));
  TEST_ASSERT_TRUE(released.enabled);
}
}

void test_startup_is_solid_and_has_highest_priority() {
  StatusLedController led;
  led.begin(1000UL);
  StatusLedOutput output =
      sample(led, 1000UL, true, true, true, true, true, true);
  TEST_ASSERT_TRUE(output.enabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::STARTUP),
                          static_cast<uint8_t>(output.mode));
  TEST_ASSERT_TRUE(
      sample(led, 5999UL, true, true, true, true, true, true).enabled);
}

void test_both_esp8266_onboard_leds_use_active_low_polarity() {
  TEST_ASSERT_TRUE(statusLedLevelHigh(false, true));
  TEST_ASSERT_FALSE(statusLedLevelHigh(true, true));
}

void test_startup_release_enters_current_mode_without_residual_pattern() {
  StatusLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 4999UL, false, false, false, false, false,
                          true).enabled);
  const StatusLedOutput released = sample(led, 5000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::WAITING_SYNC),
                          static_cast<uint8_t>(released.mode));
  TEST_ASSERT_TRUE(released.enabled);
  TEST_ASSERT_FALSE(sample(led, 5100UL).enabled);
}

void test_waiting_and_ready_periods() {
  StatusLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 600UL).enabled);
  TEST_ASSERT_FALSE(sample(led, 700UL).enabled);
  TEST_ASSERT_TRUE(sample(led, 2600UL).enabled);
  TEST_ASSERT_TRUE(sample(led, 2700UL, false, false, false, false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 2800UL, false, false, false, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5700UL, false, false, false, false, true).enabled);
}

void test_wifi_double_flash_and_preparing_fast_flash() {
  StatusLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 700UL, false, false, false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 800UL, false, false, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 900UL, false, false, false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 1000UL, false, false, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 1100UL, false, false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 1250UL, false, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 1400UL, false, false, true).enabled);
}

void test_transmission_is_solid_and_error_is_triple_flash() {
  StatusLedController led;
  led.begin(0);
  TEST_ASSERT_TRUE(sample(led, 700UL, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5000UL, false, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5100UL, true, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5200UL, true, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5300UL, true, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5400UL, true, true).enabled);
  TEST_ASSERT_TRUE(sample(led, 5500UL, true, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 5600UL, true, true).enabled);
}

void test_priority_order_is_respected() {
  StatusLedController led;
  led.begin(0);
  StatusLedOutput output = sample(led, 100UL, true, true, true, true, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::RF_ERROR),
                          static_cast<uint8_t>(output.mode));
  output = sample(led, 200UL, false, true, true, true, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::TRANSMITTING),
                          static_cast<uint8_t>(output.mode));
  output = sample(led, 300UL, false, false, true, true, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::PREPARING),
                          static_cast<uint8_t>(output.mode));
  output = sample(led, 400UL, false, false, false, true, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::WIFI_CONFIG),
                          static_cast<uint8_t>(output.mode));
}

void test_transitions_restart_pattern_without_blocking() {
  StatusLedController led;
  led.begin(UINT32_MAX - 300UL);
  TEST_ASSERT_TRUE(sample(led, UINT32_MAX - 300UL).enabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(StatusLedMode::WAITING_SYNC),
                          static_cast<uint8_t>(sample(led, 299UL).mode));
  TEST_ASSERT_TRUE(sample(led, 300UL, false, false, true).enabled);
  TEST_ASSERT_FALSE(sample(led, 450UL, false, false, true).enabled);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_both_esp8266_onboard_leds_use_active_low_polarity);
  RUN_TEST(test_startup_is_solid_and_has_highest_priority);
  RUN_TEST(test_startup_release_enters_current_mode_without_residual_pattern);
  RUN_TEST(test_hardware_test_runs_three_flashes_then_releases_cleanly);
  RUN_TEST(test_waiting_and_ready_periods);
  RUN_TEST(test_wifi_double_flash_and_preparing_fast_flash);
  RUN_TEST(test_transmission_is_solid_and_error_is_triple_flash);
  RUN_TEST(test_priority_order_is_respected);
  RUN_TEST(test_transitions_restart_pattern_without_blocking);
  return UNITY_END();
}
