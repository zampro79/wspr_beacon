#include <unity.h>

#include "PowerSaveDisplayPolicy.h"

void test_boot_sta_counts_only_visible_noncritical_time() {
  PowerSaveDisplayPolicy policy;
  policy.begin(true, 0);
  policy.setPresentation(PowerSavePresentation::STA_IPV4, 100);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
      static_cast<uint8_t>(policy.update(14100, false, true)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
      static_cast<uint8_t>(policy.update(18100, true, true)));
  policy.setPresentation(PowerSavePresentation::NONE, 18100);
  policy.update(22100, false, true);
  policy.setPresentation(PowerSavePresentation::STA_IPV4, 22100);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
      static_cast<uint8_t>(policy.update(23099, false, true)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::DISPLAY_OFF),
      static_cast<uint8_t>(policy.update(23100, false, true)));
  TEST_ASSERT_FALSE(policy.renderAllowed());
}

void test_sta_threshold_does_not_require_qualified_time() {
  PowerSaveDisplayPolicy policy;
  policy.begin(true, 0);
  policy.setPresentation(PowerSavePresentation::STA_IPV4, 0);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
      static_cast<uint8_t>(policy.update(14999, false, true)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::DISPLAY_OFF),
      static_cast<uint8_t>(policy.update(15000, false, true)));
}

void test_ap_durations_and_runtime_transitions() {
  const PowerSavePresentation modes[] = {
      PowerSavePresentation::AP_PROVISIONING,
      PowerSavePresentation::AP_OPERATIONAL,
      PowerSavePresentation::AP_START_ERROR};
  const uint32_t durations[] = {15000UL, 15000UL, 10000UL};
  for (uint8_t index = 0; index < 3; ++index) {
    PowerSaveDisplayPolicy policy;
    policy.begin(true, 0);
    policy.setPresentation(modes[index], 0);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
        static_cast<uint8_t>(policy.update(durations[index] - 1U, false, true)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PowerSaveDisplayAction::DISPLAY_OFF),
        static_cast<uint8_t>(policy.update(durations[index], false, true)));
    policy.setEnabled(false, durations[index]);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
        static_cast<uint8_t>(policy.update(durations[index] + 1U, true, true)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PowerSaveDisplayAction::DISPLAY_ON),
        static_cast<uint8_t>(policy.update(durations[index] + 2U, false, true)));
    policy.setEnabled(true, durations[index] + 3U);
    TEST_ASSERT_FALSE(policy.renderAllowed());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(PowerSaveDisplayAction::DISPLAY_OFF),
        static_cast<uint8_t>(policy.update(durations[index] + 4U, false, true)));
  }
}

void test_absent_display_is_safe_and_completes_logical_off() {
  PowerSaveDisplayPolicy policy;
  policy.begin(true, 0);
  policy.setPresentation(PowerSavePresentation::STA_IPV4, 0);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(PowerSaveDisplayAction::NONE),
      static_cast<uint8_t>(policy.update(15000, false, false)));
  TEST_ASSERT_TRUE(policy.displayOff());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_sta_counts_only_visible_noncritical_time);
  RUN_TEST(test_sta_threshold_does_not_require_qualified_time);
  RUN_TEST(test_ap_durations_and_runtime_transitions);
  RUN_TEST(test_absent_display_is_safe_and_completes_logical_off);
  return UNITY_END();
}
