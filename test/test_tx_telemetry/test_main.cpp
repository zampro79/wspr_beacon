#include <string.h>
#include <unity.h>

#include "TxTelemetry.h"

void test_frame_started_and_completed() {
  TxTelemetry telemetry;
  telemetry.prepareFrame(120000UL, TxTimeSource::GPS, "IW2NNV", "JN45", 10,
                         14097100UL);
  telemetry.frameStarted();
  telemetry.frameCompleted();
  const TxTelemetrySnapshot &snapshot = telemetry.snapshot();
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.framesStarted);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.framesCompleted);
  TEST_ASSERT_EQUAL_UINT32(0, snapshot.framesInterrupted);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxFrameResult::COMPLETED_BY_FIRMWARE),
                          static_cast<uint8_t>(snapshot.lastFrame.result));
  TEST_ASSERT_EQUAL_UINT32(14097100UL, snapshot.lastFrame.frequencyHz);
  TEST_ASSERT_EQUAL_STRING("IW2NNV", snapshot.lastFrame.callsign);
  TEST_ASSERT_EQUAL_STRING("JN45", snapshot.lastFrame.locator);
  TEST_ASSERT_EQUAL_UINT8(10, snapshot.lastFrame.powerDbm);
  TEST_ASSERT_FALSE(snapshot.lastFrame.rfVerified);
}

void test_frame_interrupted_by_rf_error() {
  TxTelemetry telemetry;
  telemetry.prepareFrame(240000UL, TxTimeSource::NTP, "AC2WZ", "FN31", 23,
                         14097120UL);
  telemetry.frameStarted();
  telemetry.frameInterrupted(TxInhibitReason::RF_RUNTIME_ERROR);
  const TxTelemetrySnapshot &snapshot = telemetry.snapshot();
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.framesStarted);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.framesInterrupted);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxFrameResult::INTERRUPTED_RF_ERROR),
                          static_cast<uint8_t>(snapshot.lastFrame.result));
  TEST_ASSERT_EQUAL_UINT32(14097120UL, snapshot.lastFrame.frequencyHz);
  TEST_ASSERT_EQUAL_STRING("AC2WZ", snapshot.lastFrame.callsign);
  TEST_ASSERT_EQUAL_STRING("FN31", snapshot.lastFrame.locator);
  TEST_ASSERT_EQUAL_UINT8(23, snapshot.lastFrame.powerDbm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::RF_RUNTIME_ERROR),
                          static_cast<uint8_t>(snapshot.lastFrame.endReason));
}

void test_time_inhibition_and_recovery_do_not_create_frames() {
  TxTelemetry telemetry;
  telemetry.setInhibitReason(TxInhibitReason::TIME_UNAVAILABLE);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::TIME_UNAVAILABLE),
                          static_cast<uint8_t>(telemetry.snapshot().inhibitReason));
  TEST_ASSERT_EQUAL_UINT32(0, telemetry.snapshot().framesStarted);
  telemetry.setInhibitReason(TxInhibitReason::NONE);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::NONE),
                          static_cast<uint8_t>(telemetry.snapshot().inhibitReason));
  TEST_ASSERT_EQUAL_UINT32(0, telemetry.snapshot().framesStarted);
}

void test_counter_overflow_saturates() {
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,
      TxTelemetry::incrementCounter(UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX,
      TxTelemetry::incrementCounter(UINT32_MAX - 1U));
}

void test_last_frame_is_immutable_while_next_frame_is_prepared() {
  TxTelemetry telemetry;
  telemetry.prepareFrame(120000UL, TxTimeSource::GPS, "IW2NNV", "JN45", 10,
                         14097100UL);
  telemetry.frameStarted();
  telemetry.frameCompleted();
  const TxFrameSnapshot previous = telemetry.snapshot().lastFrame;
  telemetry.prepareFrame(360000UL, TxTimeSource::NTP, "AC2WZ", "FN31", 23,
                         14097150UL);
  TEST_ASSERT_EQUAL_MEMORY(&previous, &telemetry.snapshot().lastFrame,
                           sizeof(previous));
}

void test_prepared_but_never_started_frame_is_not_counted() {
  TxTelemetry telemetry;
  telemetry.prepareFrame(120000UL, TxTimeSource::GPS, "IW2NNV", "JN45", 10,
                         14097100UL);
  telemetry.setInhibitReason(TxInhibitReason::SLOT_MISSED);
  TEST_ASSERT_EQUAL_UINT32(0, telemetry.snapshot().framesStarted);
  TEST_ASSERT_FALSE(telemetry.snapshot().lastFrame.valid);
}

void test_consecutive_frames_keep_identity_frequency_association() {
  TxTelemetry telemetry;
  telemetry.prepareFrame(120000UL, TxTimeSource::GPS, "IW2NNV", "JN45", 10,
                         14097011UL);
  telemetry.frameStarted();
  telemetry.frameCompleted();
  telemetry.prepareFrame(360000UL, TxTimeSource::NTP, "AC2WZ", "FN31", 23,
                         14097188UL);
  telemetry.frameStarted();
  telemetry.frameCompleted();

  const TxFrameSnapshot &last = telemetry.snapshot().lastFrame;
  TEST_ASSERT_EQUAL_STRING("AC2WZ", last.callsign);
  TEST_ASSERT_EQUAL_STRING("FN31", last.locator);
  TEST_ASSERT_EQUAL_UINT8(23, last.powerDbm);
  TEST_ASSERT_EQUAL_UINT32(14097188UL, last.frequencyHz);
  TEST_ASSERT_EQUAL_UINT32(2, telemetry.snapshot().framesStarted);
  TEST_ASSERT_EQUAL_UINT32(2, telemetry.snapshot().framesCompleted);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_frame_started_and_completed);
  RUN_TEST(test_frame_interrupted_by_rf_error);
  RUN_TEST(test_time_inhibition_and_recovery_do_not_create_frames);
  RUN_TEST(test_counter_overflow_saturates);
  RUN_TEST(test_last_frame_is_immutable_while_next_frame_is_prepared);
  RUN_TEST(test_prepared_but_never_started_frame_is_not_counted);
  RUN_TEST(test_consecutive_frames_keep_identity_frequency_association);
  return UNITY_END();
}
