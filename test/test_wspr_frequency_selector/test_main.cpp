#include <unity.h>

#include "WsprFrequencySelector.h"

void test_fixed_mode_returns_configured_frequency() {
  WsprFrequencySelector selector;
  selector.seedOnce(1234UL);
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    const uint32_t configured = band->nominalToneZeroHz - 50UL;
    TEST_ASSERT_EQUAL_UINT32(
        configured,
        selector.select(WsprFrequencyMode::FIXED, band->id, configured));
  }
}

void test_known_seed_produces_deterministic_sequence() {
  WsprFrequencySelector first;
  WsprFrequencySelector second;
  first.seedOnce(0x12345678UL);
  second.seedOnce(0x12345678UL);
  const uint32_t expected[] = {
      14097087UL, 14097021UL, 14097074UL, 14097162UL, 14097098UL};
  for (uint8_t index = 0; index < 5; ++index) {
    const uint32_t selected = first.select(
        WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0);
    TEST_ASSERT_EQUAL_UINT32(expected[index], selected);
    TEST_ASSERT_EQUAL_UINT32(
        selected,
        second.select(WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0));
  }
}

void test_random_mode_covers_complete_historical_interval() {
  WsprFrequencySelector selector;
  selector.seedOnce(0xA5A5A5A5UL);
  bool seen[WsprFrequencySelector::RANDOM_VALUE_COUNT] = {};
  uint32_t minimum = UINT32_MAX;
  uint32_t maximum = 0;
  for (uint32_t sample = 0; sample < 20000UL; ++sample) {
    const uint32_t frequency = selector.select(
        WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(14097010UL, frequency);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(14097189UL, frequency);
    seen[frequency - 14097010UL] = true;
    if (frequency < minimum) minimum = frequency;
    if (frequency > maximum) maximum = frequency;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(BandFrequencyValidation::VALID),
        static_cast<uint8_t>(BandPlan::validateToneSet(
            WsprBandId::BAND_20M, frequency, 0)));
  }
  TEST_ASSERT_EQUAL_UINT32(14097010UL, minimum);
  TEST_ASSERT_EQUAL_UINT32(14097189UL, maximum);
  for (uint16_t index = 0;
       index < WsprFrequencySelector::RANDOM_VALUE_COUNT; ++index) {
    TEST_ASSERT_TRUE(seen[index]);
  }
}

void test_rejection_sampling_has_no_evident_distribution_bias() {
  WsprFrequencySelector selector;
  selector.seedOnce(0xCAFEBABEUL);
  uint16_t counts[WsprFrequencySelector::RANDOM_VALUE_COUNT] = {};
  constexpr uint32_t SAMPLE_COUNT = 180000UL;
  for (uint32_t sample = 0; sample < SAMPLE_COUNT; ++sample) {
    const uint32_t frequency = selector.select(
        WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0);
    ++counts[frequency - 14097010UL];
  }
  for (uint16_t index = 0;
       index < WsprFrequencySelector::RANDOM_VALUE_COUNT; ++index) {
    TEST_ASSERT_GREATER_THAN_UINT16(800, counts[index]);
    TEST_ASSERT_LESS_THAN_UINT16(1200, counts[index]);
  }
}

void test_seed_is_applied_once_and_zero_has_safe_fallback() {
  WsprFrequencySelector first;
  WsprFrequencySelector second;
  first.seedOnce(0);
  first.seedOnce(99);
  second.seedOnce(0);
  TEST_ASSERT_TRUE(first.seeded());
  for (uint8_t index = 0; index < 8; ++index) {
    TEST_ASSERT_EQUAL_UINT32(
        first.select(WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0),
        second.select(WsprFrequencyMode::RANDOM, WsprBandId::BAND_20M, 0));
  }
}

void test_random_uses_each_enabled_band_nominal_and_safe_window() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    WsprFrequencySelector selector;
    selector.seedOnce(UINT32_MAX - index);
    for (uint16_t sample = 0; sample < 512; ++sample) {
      const uint32_t selected = selector.select(
          WsprFrequencyMode::RANDOM, band->id, 1UL);
      TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
          band->nominalToneZeroHz - 90UL, selected);
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(
          band->nominalToneZeroHz + 89UL, selected);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(BandFrequencyValidation::VALID),
          static_cast<uint8_t>(BandPlan::validateToneSet(
              band->id, selected, 0)));
    }
  }
  const WsprBandId unknown = static_cast<WsprBandId>(255);
  WsprFrequencySelector selector;
  TEST_ASSERT_EQUAL_UINT32(
      123456UL,
      selector.select(WsprFrequencyMode::RANDOM, unknown, 123456UL));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fixed_mode_returns_configured_frequency);
  RUN_TEST(test_known_seed_produces_deterministic_sequence);
  RUN_TEST(test_random_mode_covers_complete_historical_interval);
  RUN_TEST(test_rejection_sampling_has_no_evident_distribution_bias);
  RUN_TEST(test_seed_is_applied_once_and_zero_has_safe_fallback);
  RUN_TEST(test_random_uses_each_enabled_band_nominal_and_safe_window);
  return UNITY_END();
}
