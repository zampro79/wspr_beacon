#include <cstring>

#include <unity.h>

#include "PreparingStatusScreen.h"
#include "TxStatusScreen.h"

TxPreparedSnapshot prepared(const char *callsign, const char *locator,
                            uint8_t power, uint32_t frequency) {
  TxPreparedSnapshot result;
  std::strncpy(result.message.callsign, callsign,
               sizeof(result.message.callsign) - 1U);
  std::strncpy(result.message.locator, locator,
               sizeof(result.message.locator) - 1U);
  result.message.powerDbm = power;
  result.frequencyHz = frequency;
  result.valid = true;
  return result;
}
void test_tx_layout_uses_prepared_frame_values() {
  TxPreparedSnapshot snapshot =
      prepared("IW2NNV", "JN45", 23, 14097037UL);
  snapshot.frequencyMode = WsprFrequencyMode::RANDOM;
  const TxStatusLayout layout = TxStatusScreen::build(snapshot);
  TEST_ASSERT_EQUAL_STRING("IW2NNV JN45 23", layout.message);
  TEST_ASSERT_EQUAL_STRING("14.097037 MHz", layout.frequency);
  TEST_ASSERT_EQUAL_UINT8(22, layout.messageX);
  TEST_ASSERT_EQUAL_UINT8(25, layout.frequencyX);
}
void test_maximum_values_fit_128_by_64_without_overlap() {
  const TxStatusLayout layout =
      TxStatusScreen::build(prepared("ABCDEF", "RR99", 60, 28126100UL));
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(128,
      layout.messageX + std::strlen(layout.message) * 6U);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(128,
      layout.frequencyX + std::strlen(layout.frequency) * 6U);
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(64, TxStatusLayout::FREQUENCY_Y + 8U);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT8(
      TxStatusLayout::TX_Y + 16U, TxStatusLayout::MESSAGE_Y);
}

void test_prepared_snapshot_is_an_independent_copy() {
  TxPreparedSnapshot original = prepared("IW2NNV", "JN45", 23, 14097100UL);
  const TxPreparedSnapshot frozen = original;
  original.message.callsign[0] = 'X';
  original.message.locator[0] = 'R';
  original.frequencyHz = 1;
  TEST_ASSERT_EQUAL_STRING("IW2NNV", frozen.message.callsign);
  TEST_ASSERT_EQUAL_STRING("JN45", frozen.message.locator);
  TEST_ASSERT_EQUAL_UINT32(14097100UL, frozen.frequencyHz);
}

void test_preparing_layout_fits_maximum_values_without_overlap() {
  TxPreparedSnapshot snapshot =
      prepared("ABCDEF", "RR99", 60, 14097189UL);
  snapshot.frequencyMode = WsprFrequencyMode::RANDOM;
  const PreparingStatusLayout layout = PreparingStatusScreen::build(snapshot);
  TEST_ASSERT_EQUAL_STRING("ABCDEF RR99 60", layout.message);
  TEST_ASSERT_EQUAL_STRING("14.097189 MHz", layout.frequency);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(
      128, layout.messageX + std::strlen(layout.message) * 6U);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(
      128, layout.frequencyX + std::strlen(layout.frequency) * 6U);
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(64,
                                  PreparingStatusLayout::FREQUENCY_Y + 8U);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT8(
      PreparingStatusLayout::STATUS_Y + 16U,
      PreparingStatusLayout::MESSAGE_Y);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_tx_layout_uses_prepared_frame_values);
  RUN_TEST(test_maximum_values_fit_128_by_64_without_overlap);
  RUN_TEST(test_prepared_snapshot_is_an_independent_copy);
  RUN_TEST(test_preparing_layout_fits_maximum_values_without_overlap);
  return UNITY_END();
}
