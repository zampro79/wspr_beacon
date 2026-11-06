#include <string>
#include <cstdio>

#include <unity.h>

#include "GpsReceiver.h"
#include "GpsTimeAdapter.h"
#include "TimeService.h"
#include "WsprScheduler.h"

namespace {
uint32_t nowMillis = 0;

class FakeStream final : public Stream {
 public:
  int available() override {
    return static_cast<int>(input_.size() - offset_);
  }

  int read() override {
    if (offset_ >= input_.size()) return -1;
    return static_cast<unsigned char>(input_[offset_++]);
  }

  void append(const char *value) { input_ += value; }

  void appendNmea(const char *body) {
    uint8_t checksum = 0;
    for (const char *cursor = body; *cursor != '\0'; ++cursor) {
      checksum ^= static_cast<uint8_t>(*cursor);
    }
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), "*%02X\r\n", checksum);
    input_ += '$';
    input_ += body;
    input_ += suffix;
  }

 private:
  std::string input_;
  size_t offset_ = 0;
};

GpsTimeSample sampleFrom(const GpsReceiver &gps) {
  GpsTimeSample sample;
  sample.referenceMonotonicMillis = gps.data().lastSentenceMillis;
  sample.utcMillisOfDay =
      gps.utcMillisOfDayAt(sample.referenceMonotonicMillis);
  sample.sourceAvailable = gps.data().nmeaAvailable;
  sample.utcValid = gps.data().timeValid;
  sample.ppsPresent = gps.data().ppsAvailable;
  sample.ppsAssociated = gps.data().ppsAssociated;
  return sample;
}

void qualify(GpsReceiver &gps, uint32_t first = 1000UL) {
  gps.notePps(first);
  gps.notePps(first + 1000UL);
  gps.notePps(first + 2000UL);
  gps.notePps(first + 3000UL);
}

void test_single_arbitrary_edge_cannot_promote_gps_over_ntp() {
  FakeStream stream;
  GpsReceiver gps(stream);
  TimeService service(TimeServiceConfig(5000UL, 5000UL));
  GpsTimeAdapter adapter;

  TimeReference ntp;
  ntp.utcMillisOfDay = 45318000UL;
  ntp.quality = TimeQuality::SYNCHRONIZED;
  ntp.utcValid = true;
  TEST_ASSERT_TRUE(service.update(TimeSource::NTP, ntp, 900UL));

  nowMillis = 1000UL;
  gps.notePps(nowMillis);
  nowMillis = 1050UL;
  stream.append(
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n");
  gps.update();

  TEST_ASSERT_FALSE(gps.data().ppsAssociated);
  TEST_ASSERT_TRUE(adapter.update(service, sampleFrom(gps)));
  const TimeSnapshot snapshot = service.snapshot(nowMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeSource::NTP),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::SYNCHRONIZED),
                    static_cast<int>(snapshot.quality));
}

void test_regular_sequence_allows_valid_pps_nmea_association() {
  FakeStream stream;
  GpsReceiver gps(stream);
  qualify(gps);
  nowMillis = 4050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_TRUE(gps.data().ppsAvailable);
  TEST_ASSERT_TRUE(gps.data().ppsAssociated);
  TEST_ASSERT_EQUAL_UINT32(45319050UL, gps.utcMillisOfDayAt(nowMillis));
}

void test_duplicate_gga_rmc_epoch_does_not_require_a_new_edge() {
  FakeStream stream;
  GpsReceiver gps(stream);
  qualify(gps);
  nowMillis = 4050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  nowMillis = 4100UL;
  stream.appendNmea(
      "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W");
  gps.update();
  TEST_ASSERT_TRUE(gps.data().ppsAssociated);
}

void test_incoherent_utc_for_next_edge_revokes_qualification() {
  FakeStream stream;
  GpsReceiver gps(stream);
  qualify(gps);
  nowMillis = 4050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  gps.notePps(5000UL);
  nowMillis = 5050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_FALSE(gps.data().ppsAssociated);

  gps.notePps(6000UL);
  gps.notePps(7000UL);
  gps.notePps(8000UL);
  gps.notePps(9000UL);
  nowMillis = 9050UL;
  stream.appendNmea(
      "GPGGA,123524,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_TRUE(gps.data().ppsAssociated);
}

void test_missing_pps_downgrades_existing_reference_without_refreshing_it() {
  FakeStream stream;
  GpsReceiver gps(stream);
  GpsTimeAdapter adapter;
  TimeService service(TimeServiceConfig(5000UL, 5000UL));
  qualify(gps);
  nowMillis = 4050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_TRUE(adapter.update(service, sampleFrom(gps)));
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::PRECISE),
                    static_cast<int>(service.snapshot(nowMillis).quality));

  nowMillis = 5101UL;
  gps.update();
  TEST_ASSERT_FALSE(gps.data().ppsAssociated);
  TEST_ASSERT_TRUE(adapter.update(service, sampleFrom(gps)));
  const TimeSnapshot snapshot = service.snapshot(nowMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(TimeQuality::COARSE),
                    static_cast<int>(snapshot.quality));
  TEST_ASSERT_EQUAL_UINT32(1051UL, snapshot.referenceAgeMillis);
  TEST_ASSERT_FALSE(WsprScheduler::transmissionAuthorized(snapshot));
}

void test_multiple_edges_before_loop_do_not_qualify_receiver() {
  FakeStream stream;
  GpsReceiver gps(stream);
  gps.notePps(1000UL, 4);
  nowMillis = 1050UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_FALSE(gps.data().ppsAssociated);
}

void test_late_nmea_cannot_associate_with_expired_pps() {
  FakeStream stream;
  GpsReceiver gps(stream);
  qualify(gps);
  nowMillis = 5101UL;
  stream.appendNmea(
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  gps.update();
  TEST_ASSERT_TRUE(gps.data().timeValid);
  TEST_ASSERT_FALSE(gps.data().ppsAssociated);
}
}  // namespace

unsigned long millis() { return nowMillis; }

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_single_arbitrary_edge_cannot_promote_gps_over_ntp);
  RUN_TEST(test_regular_sequence_allows_valid_pps_nmea_association);
  RUN_TEST(test_duplicate_gga_rmc_epoch_does_not_require_a_new_edge);
  RUN_TEST(test_incoherent_utc_for_next_edge_revokes_qualification);
  RUN_TEST(test_missing_pps_downgrades_existing_reference_without_refreshing_it);
  RUN_TEST(test_multiple_edges_before_loop_do_not_qualify_receiver);
  RUN_TEST(test_late_nmea_cannot_associate_with_expired_pps);
  return UNITY_END();
}
