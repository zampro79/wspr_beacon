#include <unity.h>

#include "OledPageController.h"

namespace {
void assertPage(OledOperationalPage expected, OledOperationalPage actual) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                          static_cast<uint8_t>(actual));
}
}  // namespace

void test_unqualified_time_keeps_time_status_page() {
  OledPageController controller;
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(0, false, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(20000UL, false, false));
}

void test_qualified_time_alternates_every_five_seconds() {
  OledPageController controller;
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(1000UL, true, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(5999UL, true, false));
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(6000UL, true, false));
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(10999UL, true, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(11000UL, true, false));
}

void test_loss_and_recovery_restart_deterministically_from_time_page() {
  OledPageController controller;
  controller.update(0, true, false);
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(5000UL, true, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(5100UL, false, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(20000UL, true, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(24999UL, true, false));
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(25000UL, true, false));
}

void test_timing_critical_pause_freezes_page_and_timer() {
  OledPageController controller;
  controller.update(0, true, false);
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(4000UL, true, true));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(10000UL, true, true));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(10000UL, true, false));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(10999UL, true, false));
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(11000UL, true, false));
}

void test_qualification_loss_during_pause_applies_on_resume() {
  OledPageController controller;
  controller.update(0, true, false);
  controller.update(5000UL, true, false);
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(6000UL, false, true));
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(9000UL, false, false));
}

void test_page_timer_handles_millis_overflow() {
  OledPageController controller;
  const uint32_t start = UINT32_MAX - 1999UL;
  controller.update(start, true, false);
  assertPage(OledOperationalPage::TIME_STATUS,
             controller.update(2999UL, true, false));
  assertPage(OledOperationalPage::IDLE_STATUS,
             controller.update(3000UL, true, false));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_unqualified_time_keeps_time_status_page);
  RUN_TEST(test_qualified_time_alternates_every_five_seconds);
  RUN_TEST(test_loss_and_recovery_restart_deterministically_from_time_page);
  RUN_TEST(test_timing_critical_pause_freezes_page_and_timer);
  RUN_TEST(test_qualification_loss_during_pause_applies_on_resume);
  RUN_TEST(test_page_timer_handles_millis_overflow);
  return UNITY_END();
}
