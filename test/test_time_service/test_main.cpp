#include <unity.h>

#include "TimeService.h"

namespace {
constexpr TimeServiceConfig CONFIG = {2000UL, 5000UL};

TimeReference reference(uint32_t utcMillis, TimeQuality quality) {
  TimeReference result;
  result.utcMillisOfDay = utcMillis;
  result.quality = quality;
  result.utcValid = true;
  return result;
}
void test_gps_has_priority_over_available_ntp() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(10000, TimeQuality::COARSE), 100));
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(20000, TimeQuality::PRECISE), 200));
  const TimeSnapshot snapshot = service.snapshot(300);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_EQUAL_UINT32(20100, snapshot.utcMillisOfDay);
  TEST_ASSERT_EQUAL_UINT32(100, snapshot.referenceAgeMillis);
  TEST_ASSERT_TRUE(snapshot.sourceAvailable(TimeSource::NTP));
  TEST_ASSERT_TRUE(snapshot.sourceValid(TimeSource::NTP));
}

void test_ntp_is_selected_when_gps_is_absent() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.setSourceAvailable(TimeSource::GPS, false));
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(30000, TimeQuality::SYNCHRONIZED), 50));
  const TimeSnapshot snapshot = service.snapshot(75);
  TEST_ASSERT_TRUE(snapshot.utcValid());
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NTP),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_FALSE(snapshot.sourceAvailable(TimeSource::GPS));
}

void test_returning_gps_restores_priority() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(1000, TimeQuality::COARSE), 10));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NTP),
                    static_cast<int>(service.snapshot(20).source));
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(5000, TimeQuality::PRECISE), 30));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(service.snapshot(40).source));
}

void test_synchronized_ntp_precedes_coarse_gps_until_precise_gps_returns() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(1000, TimeQuality::COARSE), 10));
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(2000, TimeQuality::SYNCHRONIZED), 10));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NTP),
                    static_cast<int>(service.snapshot(20).source));
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(3000, TimeQuality::PRECISE), 30));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(service.snapshot(40).source));
}

void test_no_sources_means_invalid_utc() {
  TimeService service(CONFIG);
  const TimeSnapshot snapshot = service.snapshot(100);
  TEST_ASSERT_FALSE(snapshot.utcValid());
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NONE),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_FALSE(snapshot.sourceAvailable(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.sourceValid(TimeSource::NTP));
}

void test_references_expire_and_recover() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(10000, TimeQuality::PRECISE), 100));
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(20000, TimeQuality::COARSE), 100));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(service.snapshot(2100).source));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NTP),
                    static_cast<int>(service.snapshot(2101).source));
  TEST_ASSERT_FALSE(service.snapshot(5101).utcValid());
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(40000, TimeQuality::PRECISE), 5200));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(service.snapshot(5200).source));
}

void test_ntp_renewal_restarts_reference_lifetime() {
  TimeService service(TimeServiceConfig(5000UL, 3600000UL));
  TEST_ASSERT_TRUE(service.update(
      TimeSource::NTP,
      reference(10000UL, TimeQuality::SYNCHRONIZED), 100UL));
  TEST_ASSERT_TRUE(service.update(
      TimeSource::NTP,
      reference(910000UL, TimeQuality::SYNCHRONIZED), 900100UL));
  TEST_ASSERT_TRUE(service.snapshot(3600101UL).utcValid());
  TEST_ASSERT_TRUE(service.snapshot(4500100UL).utcValid());
  TEST_ASSERT_FALSE(service.snapshot(4500101UL).utcValid());
}

void test_monotonic_counter_overflow_is_supported() {
  TimeService service(CONFIG);
  const uint32_t referenceTick = UINT32_MAX - 100UL;
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(86399900UL, TimeQuality::PRECISE),
                                  referenceTick));
  const TimeSnapshot snapshot = service.snapshot(99);
  TEST_ASSERT_TRUE(snapshot.utcValid());
  TEST_ASSERT_EQUAL_UINT32(200, snapshot.referenceAgeMillis);
  TEST_ASSERT_EQUAL_UINT32(100, snapshot.utcMillisOfDay);
}

void test_utc_updates_do_not_mutate_caller_monotonic_clock() {
  TimeService service(CONFIG);
  uint32_t schedulerMonotonic = 123456UL;
  const uint32_t before = schedulerMonotonic;
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(7000, TimeQuality::PRECISE),
                                  schedulerMonotonic));
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP,
                                  reference(9000, TimeQuality::COARSE),
                                  schedulerMonotonic));
  service.snapshot(schedulerMonotonic);
  TEST_ASSERT_EQUAL_UINT32(before, schedulerMonotonic);
}

void test_invalid_samples_are_rejected_without_losing_last_reference() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(1234, TimeQuality::PRECISE), 10));
  TimeReference invalid = reference(86400000UL, TimeQuality::PRECISE);
  TEST_ASSERT_FALSE(service.update(TimeSource::GPS, invalid, 20));
  invalid = reference(2000, TimeQuality::PRECISE);
  invalid.utcValid = false;
  TEST_ASSERT_FALSE(service.update(TimeSource::GPS, invalid, 30));
  TEST_ASSERT_FALSE(service.update(TimeSource::NONE,
                                   reference(3000, TimeQuality::COARSE), 40));
  const TimeSnapshot snapshot = service.snapshot(40);
  TEST_ASSERT_EQUAL_UINT32(1264, snapshot.utcMillisOfDay);
  TEST_ASSERT_EQUAL_UINT32(30, snapshot.referenceAgeMillis);
}

void test_availability_is_distinct_from_utc_validity() {
  TimeService service(CONFIG);
  TEST_ASSERT_TRUE(service.setSourceAvailable(TimeSource::GPS, true));
  TimeSnapshot snapshot = service.snapshot(1);
  TEST_ASSERT_TRUE(snapshot.sourceAvailable(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.sourceValid(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.utcValid());

  TEST_ASSERT_TRUE(service.update(TimeSource::GPS,
                                  reference(5000, TimeQuality::UNKNOWN), 2));
  snapshot = service.snapshot(2);
  TEST_ASSERT_TRUE(snapshot.sourceValid(TimeSource::GPS));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::UNKNOWN),
                    static_cast<int>(snapshot.quality));

  TEST_ASSERT_TRUE(service.invalidate(TimeSource::GPS));
  snapshot = service.snapshot(3);
  TEST_ASSERT_TRUE(snapshot.sourceAvailable(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.sourceValid(TimeSource::GPS));
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_gps_has_priority_over_available_ntp);
  RUN_TEST(test_ntp_is_selected_when_gps_is_absent);
  RUN_TEST(test_returning_gps_restores_priority);
  RUN_TEST(test_synchronized_ntp_precedes_coarse_gps_until_precise_gps_returns);
  RUN_TEST(test_no_sources_means_invalid_utc);
  RUN_TEST(test_references_expire_and_recover);
  RUN_TEST(test_ntp_renewal_restarts_reference_lifetime);
  RUN_TEST(test_monotonic_counter_overflow_is_supported);
  RUN_TEST(test_utc_updates_do_not_mutate_caller_monotonic_clock);
  RUN_TEST(test_invalid_samples_are_rejected_without_losing_last_reference);
  RUN_TEST(test_availability_is_distinct_from_utc_validity);
  return UNITY_END();
}
