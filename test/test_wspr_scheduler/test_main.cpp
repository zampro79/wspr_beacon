#include <unity.h>

#include "TimeService.h"
#include "WsprScheduler.h"

namespace {
constexpr TimeServiceConfig CONFIG(5000UL, 3600000UL);

TimeSnapshot snapshot(TimeSource source, TimeQuality quality,
                      uint32_t utcMillis, uint32_t monotonicMillis = 1000UL) {
  TimeService service(CONFIG);
  TimeReference reference;
  reference.utcMillisOfDay = utcMillis;
  reference.quality = quality;
  reference.utcValid = true;
  service.update(source, reference, monotonicMillis);
  return service.snapshot(monotonicMillis);
}

void test_precise_gps_authorizes_slot() {
  WsprScheduler scheduler;
  const TimeSnapshot time =
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 210000UL);
  TEST_ASSERT_TRUE(WsprScheduler::transmissionAuthorized(time));
  TEST_ASSERT_EQUAL(static_cast<int>(WsprTimeAuthorization::GPS_PRECISE),
                    static_cast<int>(WsprScheduler::authorizationFor(time)));
  TEST_ASSERT_TRUE(scheduler.arm(time, 5000UL, 4, 30));
  TEST_ASSERT_EQUAL_UINT32(1, scheduler.slot());
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::WAIT),
                    static_cast<int>(scheduler.startDecision(34999UL)));
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::START),
                    static_cast<int>(scheduler.startDecision(35000UL)));
}

void test_gps_without_pps_is_not_authorized() {
  const TimeSnapshot time =
      snapshot(TimeSource::GPS, TimeQuality::COARSE, 210000UL);
  TEST_ASSERT_FALSE(WsprScheduler::transmissionAuthorized(time));
  WsprScheduler scheduler;
  TEST_ASSERT_FALSE(scheduler.arm(time, 5000UL, 4, 30));
}

void test_ntp_is_valid_and_experimentally_authorized_for_tx() {
  const TimeSnapshot time =
      snapshot(TimeSource::NTP, TimeQuality::SYNCHRONIZED, 210000UL);
  TEST_ASSERT_TRUE(time.utcValid());
  TEST_ASSERT_TRUE(WsprScheduler::transmissionAuthorized(time));
  TEST_ASSERT_EQUAL(
      static_cast<int>(WsprTimeAuthorization::NTP_EXPERIMENTAL),
      static_cast<int>(WsprScheduler::authorizationFor(time)));
  WsprScheduler scheduler;
  TEST_ASSERT_TRUE(scheduler.arm(time, 5000UL, 4, 30));
  TEST_ASSERT_EQUAL(
      static_cast<int>(WsprTimeAuthorization::NTP_EXPERIMENTAL),
      static_cast<int>(scheduler.authorization()));
}

void test_expired_ntp_is_not_authorized() {
  TimeService service(TimeServiceConfig(5000UL, 1000UL));
  TimeReference reference;
  reference.utcMillisOfDay = 210000UL;
  reference.quality = TimeQuality::SYNCHRONIZED;
  reference.utcValid = true;
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP, reference, 100UL));
  TEST_ASSERT_FALSE(WsprScheduler::transmissionAuthorized(
      service.snapshot(1101UL)));
}

void test_reference_loss_before_slot_can_cancel_arm() {
  WsprScheduler scheduler;
  TEST_ASSERT_TRUE(scheduler.arm(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 210000UL),
      5000UL, 4, 30));
  scheduler.disarm();
  TEST_ASSERT_FALSE(scheduler.armed());
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::WAIT),
                    static_cast<int>(scheduler.startDecision(35000UL)));
}

void test_source_changes_do_not_change_running_symbol_clock() {
  WsprScheduler scheduler;
  scheduler.startSymbols(1000UL);
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_TRUE(scheduler.symbolDue(1000UL));
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_EQUAL_UINT16(0, index);

  const TimeSnapshot ntp =
      snapshot(TimeSource::NTP, TimeQuality::SYNCHRONIZED, 120000UL);
  TEST_ASSERT_TRUE(WsprScheduler::transmissionAuthorized(ntp));
  TEST_ASSERT_FALSE(scheduler.symbolDue(1000UL +
                                        WsprScheduler::SYMBOL_PERIOD_US - 1));
  TEST_ASSERT_TRUE(scheduler.symbolDue(1000UL +
                                       WsprScheduler::SYMBOL_PERIOD_US));
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_EQUAL_UINT16(1, index);
}

void test_ntp_loss_during_tx_does_not_change_symbol_clock() {
  WsprScheduler scheduler;
  scheduler.startSymbols(2000UL);
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));

  TimeService service(TimeServiceConfig(5000UL, 10UL));
  TimeReference reference;
  reference.utcMillisOfDay = 120000UL;
  reference.quality = TimeQuality::SYNCHRONIZED;
  reference.utcValid = true;
  service.update(TimeSource::NTP, reference, 0);
  TEST_ASSERT_FALSE(WsprScheduler::transmissionAuthorized(
      service.snapshot(11UL)));
  TEST_ASSERT_TRUE(scheduler.symbolDue(
      2000UL + WsprScheduler::SYMBOL_PERIOD_US));
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_EQUAL_UINT16(1, index);
}

void test_ntp_resynchronization_during_tx_does_not_jump_symbols() {
  WsprScheduler scheduler;
  scheduler.startSymbols(3000UL);
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  const TimeSnapshot refreshed =
      snapshot(TimeSource::NTP, TimeQuality::SYNCHRONIZED, 240000UL);
  TEST_ASSERT_TRUE(WsprScheduler::transmissionAuthorized(refreshed));
  TEST_ASSERT_FALSE(scheduler.symbolDue(
      3000UL + WsprScheduler::SYMBOL_PERIOD_US - 1UL));
  TEST_ASSERT_TRUE(scheduler.symbolDue(
      3000UL + WsprScheduler::SYMBOL_PERIOD_US));
}

void test_late_reference_waits_for_next_slot() {
  WsprScheduler scheduler;
  const TimeSnapshot afterSlot =
      snapshot(TimeSource::NTP, TimeQuality::SYNCHRONIZED, 121000UL);
  TEST_ASSERT_FALSE(scheduler.arm(afterSlot, 5000UL, 2, 30));
  TEST_ASSERT_FALSE(scheduler.armed());
}

void test_authorization_is_frozen_when_preparation_is_armed() {
  WsprScheduler scheduler;
  TEST_ASSERT_TRUE(scheduler.arm(
      snapshot(TimeSource::NTP, TimeQuality::SYNCHRONIZED, 210000UL),
      5000UL, 4, 30));
  const TimeSnapshot returnedGps =
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 210500UL);
  TEST_ASSERT_EQUAL(static_cast<int>(WsprTimeAuthorization::GPS_PRECISE),
                    static_cast<int>(WsprScheduler::authorizationFor(returnedGps)));
  TEST_ASSERT_EQUAL(
      static_cast<int>(WsprTimeAuthorization::NTP_EXPERIMENTAL),
      static_cast<int>(scheduler.authorization()));
}

void test_slots_and_midnight_wrap() {
  WsprScheduler scheduler;
  const TimeSnapshot beforeMidnight = snapshot(
      TimeSource::GPS, TimeQuality::PRECISE, 86390000UL);
  TEST_ASSERT_TRUE(scheduler.arm(beforeMidnight, 100UL, 4, 30));
  TEST_ASSERT_EQUAL_UINT32(360, scheduler.slot());
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::START),
                    static_cast<int>(scheduler.startDecision(10100UL)));

  scheduler.disarm();
  const TimeSnapshot afterMidnight =
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 230000UL);
  TEST_ASSERT_TRUE(scheduler.arm(afterMidnight, 20000UL, 4, 30));
  TEST_ASSERT_EQUAL_UINT32(1, scheduler.slot());
}

void test_even_utc_slot_and_configured_interval() {
  WsprScheduler scheduler;
  const TimeSnapshot time =
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 119000UL);
  TEST_ASSERT_TRUE(scheduler.arm(time, 5000UL, 2, 30));
  TEST_ASSERT_EQUAL_UINT32(1, scheduler.slot());
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::WAIT),
                    static_cast<int>(scheduler.startDecision(5999UL)));
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::START),
                    static_cast<int>(scheduler.startDecision(6000UL)));
}

void test_next_slot_snapshot_matches_scheduler_cycle_and_midnight() {
  uint32_t nextSlot = UINT32_MAX;
  TEST_ASSERT_TRUE(WsprScheduler::nextSlotUtcMillis(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 45318000UL),
      4, nextSlot));
  TEST_ASSERT_EQUAL_UINT32(45360000UL, nextSlot);
  TEST_ASSERT_TRUE(WsprScheduler::nextSlotUtcMillis(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 86399000UL),
      4, nextSlot));
  TEST_ASSERT_EQUAL_UINT32(0, nextSlot);
  TimeSnapshot invalid;
  TEST_ASSERT_FALSE(WsprScheduler::nextSlotUtcMillis(invalid, 4, nextSlot));
  TEST_ASSERT_FALSE(WsprScheduler::nextSlotUtcMillis(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 0), 0, nextSlot));
}

void test_armed_slot_exposes_wrap_safe_monotonic_margin() {
  WsprScheduler scheduler;
  const uint32_t nearWrap = UINT32_MAX - 9999UL;
  TEST_ASSERT_TRUE(scheduler.arm(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 230000UL),
      nearWrap, 4, 30));
  uint32_t remaining = UINT32_MAX;
  TEST_ASSERT_TRUE(scheduler.millisUntilSlot(nearWrap, remaining));
  TEST_ASSERT_EQUAL_UINT32(10000UL, remaining);
  TEST_ASSERT_TRUE(scheduler.millisUntilSlot(0, remaining));
  TEST_ASSERT_EQUAL_UINT32(0, remaining);
  scheduler.disarm();
  TEST_ASSERT_FALSE(scheduler.millisUntilSlot(0, remaining));
}

void test_monotonic_overflow_for_slot_and_symbols() {
  WsprScheduler scheduler;
  const uint32_t nearWrap = UINT32_MAX - 9999UL;
  TEST_ASSERT_TRUE(scheduler.arm(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 230000UL),
      nearWrap, 4, 30));
  TEST_ASSERT_EQUAL(static_cast<int>(WsprStartDecision::START),
                    static_cast<int>(scheduler.startDecision(0)));

  scheduler.startSymbols(UINT32_MAX - 100UL);
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_FALSE(scheduler.symbolDue(100UL));
  TEST_ASSERT_TRUE(scheduler.symbolDue(
      static_cast<uint32_t>(UINT32_MAX - 100UL +
                            WsprScheduler::SYMBOL_PERIOD_US)));
}

void test_exactly_162_symbols_remain_ordered() {
  WsprScheduler scheduler;
  scheduler.startSymbols(0);
  for (uint16_t expected = 0; expected < WsprScheduler::SYMBOL_COUNT;
       ++expected) {
    uint16_t index = UINT16_MAX;
    TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
    TEST_ASSERT_EQUAL_UINT16(expected, index);
  }
  uint16_t extra = 0;
  TEST_ASSERT_TRUE(scheduler.symbolsComplete());
  TEST_ASSERT_FALSE(scheduler.takeSymbol(extra));
}

void test_prepared_symbol_zero_is_not_programmed_twice() {
  WsprScheduler scheduler;
  const TimeSnapshot time =
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 210000UL);
  TEST_ASSERT_TRUE(scheduler.arm(time, 5000UL, 1000UL, 4, 30));
  scheduler.startPreparedSymbols();
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_FALSE(scheduler.symbolDue(30683666UL));
  TEST_ASSERT_TRUE(scheduler.symbolDue(30683667UL));
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_EQUAL_UINT16(1, index);
  for (uint16_t expected = 2; expected < WsprScheduler::SYMBOL_COUNT;
       ++expected) {
    TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
    TEST_ASSERT_EQUAL_UINT16(expected, index);
  }
  TEST_ASSERT_TRUE(scheduler.symbolsComplete());
}

void test_prepared_symbol_deadline_handles_micros_overflow() {
  WsprScheduler scheduler;
  const uint32_t nearWrapMicros = UINT32_MAX - 999999UL;
  TEST_ASSERT_TRUE(scheduler.arm(
      snapshot(TimeSource::GPS, TimeQuality::PRECISE, 239000UL),
      100UL, nearWrapMicros, 4, 30));
  scheduler.startPreparedSymbols();
  const uint32_t symbolOneDeadline = static_cast<uint32_t>(
      nearWrapMicros + 1000000ULL + WsprScheduler::SYMBOL_PERIOD_US);
  TEST_ASSERT_FALSE(scheduler.symbolDue(symbolOneDeadline - 1UL));
  TEST_ASSERT_TRUE(scheduler.symbolDue(symbolOneDeadline));
  uint16_t index = UINT16_MAX;
  TEST_ASSERT_TRUE(scheduler.takeSymbol(index));
  TEST_ASSERT_EQUAL_UINT16(1, index);
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_precise_gps_authorizes_slot);
  RUN_TEST(test_gps_without_pps_is_not_authorized);
  RUN_TEST(test_ntp_is_valid_and_experimentally_authorized_for_tx);
  RUN_TEST(test_expired_ntp_is_not_authorized);
  RUN_TEST(test_reference_loss_before_slot_can_cancel_arm);
  RUN_TEST(test_source_changes_do_not_change_running_symbol_clock);
  RUN_TEST(test_ntp_loss_during_tx_does_not_change_symbol_clock);
  RUN_TEST(test_ntp_resynchronization_during_tx_does_not_jump_symbols);
  RUN_TEST(test_late_reference_waits_for_next_slot);
  RUN_TEST(test_authorization_is_frozen_when_preparation_is_armed);
  RUN_TEST(test_slots_and_midnight_wrap);
  RUN_TEST(test_even_utc_slot_and_configured_interval);
  RUN_TEST(test_next_slot_snapshot_matches_scheduler_cycle_and_midnight);
  RUN_TEST(test_armed_slot_exposes_wrap_safe_monotonic_margin);
  RUN_TEST(test_monotonic_overflow_for_slot_and_symbols);
  RUN_TEST(test_exactly_162_symbols_remain_ordered);
  RUN_TEST(test_prepared_symbol_zero_is_not_programmed_twice);
  RUN_TEST(test_prepared_symbol_deadline_handles_micros_overflow);
  return UNITY_END();
}
