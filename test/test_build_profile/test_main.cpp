#include <string.h>

#include <unity.h>

#include "BeaconConfig.h"
#include "BeaconSettings.h"
#include "BuildProfile.h"

void test_selected_profile_is_explicit() {
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprBuildProfile::DEVELOPMENT),
      static_cast<uint8_t>(WSPR_BUILD_PROFILE));
  TEST_ASSERT_EQUAL_STRING("DEVELOPMENT", WSPR_BUILD_PROFILE_NAME);
#elif defined(WSPR_PROFILE_PUBLIC)
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBuildProfile::PUBLIC),
                          static_cast<uint8_t>(WSPR_BUILD_PROFILE));
  TEST_ASSERT_EQUAL_STRING("PUBLIC", WSPR_BUILD_PROFILE_NAME);
#else
  TEST_FAIL_MESSAGE("Native tests require an explicit DEVELOPMENT or PUBLIC profile");
#endif
}

void test_profile_defaults_are_explicit() {
  const BeaconSettings settings;
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_EQUAL_STRING("IW2NNV", settings.callsign);
#elif defined(WSPR_PROFILE_PUBLIC)
  TEST_ASSERT_EQUAL_STRING("", settings.callsign);
#endif
  TEST_ASSERT_EQUAL_STRING("JN45", settings.fixedLocator);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BandPlan::DEFAULT_BAND),
                          static_cast<uint8_t>(settings.band));
  TEST_ASSERT_EQUAL_UINT32(BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ,
                           settings.baseFrequencyHz);
}

void test_runtime_and_persistent_defaults_are_consistent() {
  const BeaconSettings settings;
  const BeaconConfig config;
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, settings.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, config.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_LOCATOR, settings.fixedLocator);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_LOCATOR, config.fixedLocator);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(settings.band),
                          static_cast<uint8_t>(config.band));
  TEST_ASSERT_EQUAL_UINT32(settings.baseFrequencyHz, config.baseFrequencyHz);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_selected_profile_is_explicit);
  RUN_TEST(test_profile_defaults_are_explicit);
  RUN_TEST(test_runtime_and_persistent_defaults_are_consistent);
  return UNITY_END();
}
