#include <unity.h>

#include <string.h>

#include "BandPlan.h"

namespace {
struct ExpectedBand {
  WsprBandId id;
  const char *name;
  uint32_t nominalHz;
  uint32_t lowerHz;
  uint32_t upperHz;
  bool txEnabled;
};

const ExpectedBand EXPECTED[] = {
    {WsprBandId::BAND_40M, "40m", 7040100UL, 7040000UL, 7040200UL, true},
    {WsprBandId::BAND_30M, "30m", 10140200UL, 10140100UL, 10140300UL, true},
    {WsprBandId::BAND_20M, "20m", 14097100UL, 14097000UL, 14097200UL, true},
    {WsprBandId::BAND_17M, "17m", 18106100UL, 18106000UL, 18106200UL, true},
    {WsprBandId::BAND_15M, "15m", 21096100UL, 21096000UL, 21096200UL, true},
    {WsprBandId::BAND_12M, "12m", 24926100UL, 24926000UL, 24926200UL, true},
    {WsprBandId::BAND_10M, "10m", 28126100UL, 28126000UL, 28126200UL, true},
};
}  // namespace

void test_band_table_has_stable_ids_names_frequencies_and_windows() {
  TEST_ASSERT_EQUAL_UINT8(7, BandPlan::size());
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    TEST_ASSERT_NOT_NULL(band);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EXPECTED[index].id),
                            static_cast<uint8_t>(band->id));
    TEST_ASSERT_EQUAL_STRING(EXPECTED[index].name, band->name);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED[index].nominalHz,
                             band->nominalToneZeroHz);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED[index].lowerHz, band->lowerLimitHz);
    TEST_ASSERT_EQUAL_UINT32(EXPECTED[index].upperHz, band->upperLimitHz);
    TEST_ASSERT_EQUAL(EXPECTED[index].txEnabled, band->txEnabled);
  }
  TEST_ASSERT_NULL(BandPlan::at(BandPlan::size()));
}

void test_all_four_tones_are_validated_against_each_band_window() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(BandFrequencyValidation::VALID),
        static_cast<uint8_t>(BandPlan::validateToneSet(
            band->id, band->nominalToneZeroHz, 0)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(BandFrequencyValidation::VALID),
        static_cast<uint8_t>(BandPlan::validateToneSet(
            band->id, band->lowerLimitHz, 0)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(BandFrequencyValidation::VALID),
        static_cast<uint8_t>(BandPlan::validateToneSet(
            band->id, band->upperLimitHz - 5UL, 0)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(BandFrequencyValidation::OUTSIDE_WINDOW),
        static_cast<uint8_t>(BandPlan::validateToneSet(
            band->id, band->upperLimitHz - 4UL, 0)));
  }
}

void test_unknown_band_is_rejected() {
  const WsprBandId unknown = static_cast<WsprBandId>(255);
  TEST_ASSERT_NULL(BandPlan::find(unknown));
  TEST_ASSERT_FALSE(BandPlan::txEnabled(unknown));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(BandFrequencyValidation::UNKNOWN_BAND),
      static_cast<uint8_t>(BandPlan::validateToneSet(unknown, 14097100UL, 0)));
}

void test_all_defined_bands_are_authorized_for_transmission() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    TEST_ASSERT_TRUE(BandPlan::txEnabled(band->id));
  }
}

void test_frequency_and_tx_authorization_are_independent() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(BandFrequencyValidation::VALID),
      static_cast<uint8_t>(BandPlan::validateFrequency(
          WsprBandId::BAND_40M, 7040100UL)));
  TEST_ASSERT_TRUE(BandPlan::txEnabled(WsprBandId::BAND_40M));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(BandFrequencyValidation::OUTSIDE_WINDOW),
      static_cast<uint8_t>(BandPlan::validateFrequency(
          WsprBandId::BAND_20M, 14097201UL)));
  TEST_ASSERT_TRUE(BandPlan::txEnabled(WsprBandId::BAND_20M));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_band_table_has_stable_ids_names_frequencies_and_windows);
  RUN_TEST(test_all_four_tones_are_validated_against_each_band_window);
  RUN_TEST(test_unknown_band_is_rejected);
  RUN_TEST(test_all_defined_bands_are_authorized_for_transmission);
  RUN_TEST(test_frequency_and_tx_authorization_are_independent);
  return UNITY_END();
}
