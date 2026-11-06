#include <unity.h>

#include "GpsTimeAdapter.h"

namespace {
constexpr TimeServiceConfig CONFIG = {5000UL, 0};

GpsTimeSample sample(uint32_t utcMillis, uint32_t monotonicMillis,
                     bool ppsPresent = false, bool ppsAssociated = false) {
  GpsTimeSample result;
  result.utcMillisOfDay = utcMillis;
  result.referenceMonotonicMillis = monotonicMillis;
  result.sourceAvailable = true;
  result.utcValid = true;
  result.ppsPresent = ppsPresent;
  result.ppsAssociated = ppsAssociated;
  return result;
}

void test_valid_gps_sample_is_forwarded() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  TEST_ASSERT_TRUE(adapter.update(service, sample(10000, 100)));
  const TimeSnapshot snapshot = service.snapshot(150);
  TEST_ASSERT_TRUE(snapshot.utcValid());
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::GPS),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_EQUAL_UINT32(10050, snapshot.utcMillisOfDay);
  TEST_ASSERT_EQUAL_UINT32(50, snapshot.referenceAgeMillis);
}

void test_invalid_gps_sample_is_not_accepted() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  GpsTimeSample invalid = sample(10000, 100);
  invalid.utcValid = false;
  TEST_ASSERT_FALSE(adapter.update(service, invalid));
  const TimeSnapshot snapshot = service.snapshot(100);
  TEST_ASSERT_TRUE(snapshot.sourceAvailable(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.sourceValid(TimeSource::GPS));
  TEST_ASSERT_FALSE(snapshot.utcValid());
}

void test_gps_sample_expires_and_recovers() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  TEST_ASSERT_TRUE(adapter.update(service, sample(20000, 100)));
  TEST_ASSERT_TRUE(service.snapshot(5100).utcValid());
  TEST_ASSERT_FALSE(service.snapshot(5101).utcValid());
  TEST_ASSERT_TRUE(adapter.update(service, sample(30000, 5200)));
  TEST_ASSERT_TRUE(service.snapshot(5200).utcValid());
  TEST_ASSERT_EQUAL_UINT32(30000, service.snapshot(5200).utcMillisOfDay);
}

void test_pps_must_be_present_and_associated_for_precise_quality() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  TEST_ASSERT_TRUE(adapter.update(service, sample(1000, 10, false, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::COARSE),
                    static_cast<int>(service.snapshot(10).quality));
  TEST_ASSERT_TRUE(adapter.update(service, sample(2000, 20, true, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::COARSE),
                    static_cast<int>(service.snapshot(20).quality));
  TEST_ASSERT_TRUE(adapter.update(service, sample(3000, 30, true, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::PRECISE),
                    static_cast<int>(service.snapshot(30).quality));
}

void test_source_loss_invalidates_reference_until_new_sample() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  TEST_ASSERT_TRUE(adapter.update(service, sample(1000, 10)));
  GpsTimeSample unavailable;
  TEST_ASSERT_FALSE(adapter.update(service, unavailable));
  TEST_ASSERT_FALSE(service.snapshot(20).utcValid());
  unavailable.sourceAvailable = true;
  TEST_ASSERT_FALSE(adapter.update(service, unavailable));
  TEST_ASSERT_FALSE(service.snapshot(30).utcValid());
  TEST_ASSERT_TRUE(adapter.update(service, sample(4000, 40)));
  TEST_ASSERT_TRUE(service.snapshot(40).utcValid());
}

void test_same_gps_reference_is_not_refreshed_each_loop() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  const GpsTimeSample first = sample(1000, 10);
  TEST_ASSERT_TRUE(adapter.update(service, first));
  TEST_ASSERT_FALSE(adapter.update(service, first));
  TEST_ASSERT_EQUAL_UINT32(90, service.snapshot(100).referenceAgeMillis);
}

void test_same_reference_is_forwarded_when_pps_quality_is_revoked() {
  TimeService service(CONFIG);
  GpsTimeAdapter adapter;
  TEST_ASSERT_TRUE(adapter.update(service, sample(1000, 10, true, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::PRECISE),
                    static_cast<int>(service.snapshot(20).quality));
  TEST_ASSERT_TRUE(adapter.update(service, sample(1000, 10, true, false)));
  const TimeSnapshot snapshot = service.snapshot(20);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::COARSE),
                    static_cast<int>(snapshot.quality));
  TEST_ASSERT_EQUAL_UINT32(10, snapshot.referenceAgeMillis);
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_gps_sample_is_forwarded);
  RUN_TEST(test_invalid_gps_sample_is_not_accepted);
  RUN_TEST(test_gps_sample_expires_and_recovers);
  RUN_TEST(test_pps_must_be_present_and_associated_for_precise_quality);
  RUN_TEST(test_source_loss_invalidates_reference_until_new_sample);
  RUN_TEST(test_same_gps_reference_is_not_refreshed_each_loop);
  RUN_TEST(test_same_reference_is_forwarded_when_pps_quality_is_revoked);
  return UNITY_END();
}
