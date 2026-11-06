#include <unity.h>

#include "TxDisplayCoordinator.h"

void test_preparing_and_tx_on_render_once_at_their_thresholds() {
  TxDisplayCoordinator coordinator;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::RENDER_PREPARING),
      static_cast<uint8_t>(coordinator.update(true, true, true, 5000UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxDisplayAction::NONE),
                          static_cast<uint8_t>(coordinator.update(
                              true, true, true, 4000UL)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::RENDER_TX_ON),
      static_cast<uint8_t>(coordinator.update(true, true, true, 1000UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxDisplayAction::NONE),
                          static_cast<uint8_t>(coordinator.update(
                              true, true, true, 900UL)));
}

void test_tx_on_uses_the_500_ms_safety_margin() {
  TxDisplayCoordinator safe;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::RENDER_TX_ON),
      static_cast<uint8_t>(safe.update(true, true, true, 500UL)));

  TxDisplayCoordinator late;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::SKIP),
      static_cast<uint8_t>(late.update(true, true, true, 499UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxDisplayAction::NONE),
                          static_cast<uint8_t>(late.update(
                              true, true, true, 100UL)));
}

void test_absent_oled_skips_without_repeated_attempts() {
  TxDisplayCoordinator coordinator;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::SKIP),
      static_cast<uint8_t>(coordinator.update(true, false, true, 5000UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxDisplayAction::NONE),
                          static_cast<uint8_t>(coordinator.update(
                              true, false, true, 4000UL)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::SKIP),
      static_cast<uint8_t>(coordinator.update(true, false, true, 1000UL)));
}

void test_late_qualification_prioritizes_tx_on_and_reset_allows_next_frame() {
  TxDisplayCoordinator coordinator;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::RENDER_TX_ON),
      static_cast<uint8_t>(coordinator.update(true, true, true, 800UL)));
  coordinator.reset();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::RENDER_PREPARING),
      static_cast<uint8_t>(coordinator.update(true, true, true, 5000UL)));
}

void test_invalid_margin_skips_both_pages() {
  TxDisplayCoordinator coordinator;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxDisplayAction::SKIP),
      static_cast<uint8_t>(coordinator.update(true, true, false, 5000UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxDisplayAction::NONE),
                          static_cast<uint8_t>(coordinator.update(
                              true, true, true, 1000UL)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_preparing_and_tx_on_render_once_at_their_thresholds);
  RUN_TEST(test_tx_on_uses_the_500_ms_safety_margin);
  RUN_TEST(test_absent_oled_skips_without_repeated_attempts);
  RUN_TEST(test_late_qualification_prioritizes_tx_on_and_reset_allows_next_frame);
  RUN_TEST(test_invalid_margin_skips_both_pages);
  return UNITY_END();
}
