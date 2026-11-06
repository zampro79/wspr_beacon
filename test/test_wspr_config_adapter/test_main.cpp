#include <string.h>
#include <unity.h>

#include "WsprConfigAdapter.h"

void test_valid_settings_apply_every_radio_field() {
  BeaconConfig target;
  BeaconSettings settings;
  strcpy(settings.callsign, "IZ2ABC");
  strcpy(settings.fixedLocator, "AA00");
  settings.powerDbm = 37;
  settings.transmitIntervalMinutes = 6;
  settings.baseFrequencyHz = 14097050UL;
  settings.frequencyOffsetHz = 25;
  settings.si5351Correction = -12345;
  settings.frequencyMode = WsprFrequencyMode::RANDOM;
  TEST_ASSERT_TRUE(WsprConfigAdapter::apply(settings, false, target));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", target.callsign);
  TEST_ASSERT_EQUAL_STRING("AA00", target.fixedLocator);
  TEST_ASSERT_EQUAL_UINT8(37, target.powerDbm);
  TEST_ASSERT_EQUAL_UINT8(6, target.transmitIntervalMinutes);
  TEST_ASSERT_EQUAL_UINT32(14097050UL, target.baseFrequencyHz);
  TEST_ASSERT_EQUAL_INT16(25, target.frequencyOffsetHz);
  TEST_ASSERT_EQUAL_INT32(-12345, target.si5351Correction);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
                          static_cast<uint8_t>(target.band));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(target.frequencyMode));
}

void test_invalid_settings_leave_active_configuration_unchanged() {
  BeaconConfig target;
  BeaconConfig before = target;
  BeaconSettings invalid;
  invalid.baseFrequencyHz = 14097200UL;
  invalid.frequencyOffsetHz = 0;
  TEST_ASSERT_FALSE(WsprConfigAdapter::apply(invalid, false, target));
  TEST_ASSERT_EQUAL_MEMORY(&before, &target, sizeof(target));
}

void test_busy_transmitter_preserves_prepared_frame_configuration() {
  BeaconConfig target;
  BeaconConfig before = target;
  BeaconSettings changed;
  strcpy(changed.callsign, "IZ2ABC");
  changed.si5351Correction = 12345;
  TEST_ASSERT_FALSE(WsprConfigAdapter::apply(changed, true, target));
  TEST_ASSERT_EQUAL_MEMORY(&before, &target, sizeof(target));
}

void test_frequency_and_calibration_boundaries_are_applied() {
  BeaconConfig target;
  BeaconSettings settings;
  strcpy(settings.callsign, "IW2NNV");
  settings.baseFrequencyHz = 14097000UL;
  settings.frequencyOffsetHz = 0;
  settings.si5351Correction = 1000000;
  TEST_ASSERT_TRUE(WsprConfigAdapter::apply(settings, false, target));
  TEST_ASSERT_EQUAL_UINT32(14097000UL, target.baseFrequencyHz);
  TEST_ASSERT_EQUAL_INT32(1000000, target.si5351Correction);
  settings.baseFrequencyHz = 14097195UL;
  settings.frequencyOffsetHz = 0;
  TEST_ASSERT_TRUE(WsprConfigAdapter::apply(settings, false, target));
  settings.baseFrequencyHz = 14097196UL;
  TEST_ASSERT_FALSE(WsprConfigAdapter::apply(settings, false, target));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_settings_apply_every_radio_field);
  RUN_TEST(test_invalid_settings_leave_active_configuration_unchanged);
  RUN_TEST(test_busy_transmitter_preserves_prepared_frame_configuration);
  RUN_TEST(test_frequency_and_calibration_boundaries_are_applied);
  return UNITY_END();
}
