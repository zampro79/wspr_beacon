#include <unity.h>

#include "DiagnosticRegistry.h"

static DiagnosticRecord readRecord(const DiagnosticRegistry &registry,
                                   DiagnosticModule module) {
  DiagnosticRecord record;
  TEST_ASSERT_TRUE(registry.read(module, record));
  return record;
}

static void test_initial_state_is_unknown_and_not_evaluated() {
  DiagnosticRegistry registry;
  for (size_t index = 0; index < DIAGNOSTIC_MODULE_COUNT; ++index) {
    const DiagnosticRecord record =
        readRecord(registry, static_cast<DiagnosticModule>(index));
    TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_UNKNOWN),
                      static_cast<int>(record.state));
    TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::NOT_EVALUATED),
                      static_cast<int>(record.reason));
    TEST_ASSERT_EQUAL_UINT32(0, record.lastChangeMillis);
    TEST_ASSERT_EQUAL_UINT16(0, record.counters.stateChanges);
    TEST_ASSERT_EQUAL_UINT16(0, record.counters.successes);
    TEST_ASSERT_EQUAL_UINT16(0, record.counters.failures);
  }
}

static void test_all_modules_and_states_are_representable() {
  const DiagnosticModule modules[] = {
      DiagnosticModule::GPS, DiagnosticModule::PPS, DiagnosticModule::SI5351A,
      DiagnosticModule::OLED, DiagnosticModule::WIFI, DiagnosticModule::NTP};
  const DiagnosticState states[] = {
      DiagnosticState::STATE_UNKNOWN, DiagnosticState::STATE_DISABLED,
      DiagnosticState::STATE_NOT_DETECTED, DiagnosticState::STATE_INITIALIZING,
      DiagnosticState::STATE_OPERATIONAL, DiagnosticState::STATE_DEGRADED,
      DiagnosticState::STATE_ERROR};
  DiagnosticRegistry registry;
  for (size_t index = 0; index < DIAGNOSTIC_MODULE_COUNT; ++index) {
    TEST_ASSERT_TRUE(registry.setState(modules[index], states[index + 1],
                                      DiagnosticReason::NONE,
                                      static_cast<uint32_t>(index + 10)));
  }
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS, states[0],
                                    DiagnosticReason::NONE, 20));
}

static void test_unknown_is_distinct_from_not_detected() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::OLED,
                                    DiagnosticState::STATE_NOT_DETECTED,
                                    DiagnosticReason::DEVICE_NOT_FOUND, 125));
  const DiagnosticRecord record = readRecord(registry, DiagnosticModule::OLED);
  TEST_ASSERT_NOT_EQUAL(static_cast<int>(DiagnosticState::STATE_UNKNOWN),
                        static_cast<int>(record.state));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_NOT_DETECTED),
                    static_cast<int>(record.state));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::DEVICE_NOT_FOUND),
                    static_cast<int>(record.reason));
}

static void test_state_transition_updates_reason_timestamp_and_counter() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS,
                                    DiagnosticState::STATE_INITIALIZING,
                                    DiagnosticReason::NONE, 100));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS,
                                    DiagnosticState::STATE_ERROR,
                                    DiagnosticReason::COMMUNICATION_FAILURE, 250));
  const DiagnosticRecord record = readRecord(registry, DiagnosticModule::GPS);
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_ERROR),
                    static_cast<int>(record.state));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::COMMUNICATION_FAILURE),
                    static_cast<int>(record.reason));
  TEST_ASSERT_EQUAL_UINT32(250, record.lastChangeMillis);
  TEST_ASSERT_EQUAL_UINT16(2, record.counters.stateChanges);
}

static void test_repeated_state_does_not_create_a_transition() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::NTP,
                                    DiagnosticState::STATE_DEGRADED,
                                    DiagnosticReason::TIMEOUT, 100));
  TEST_ASSERT_FALSE(registry.setState(DiagnosticModule::NTP,
                                     DiagnosticState::STATE_DEGRADED,
                                     DiagnosticReason::TIMEOUT, 900));
  const DiagnosticRecord record = readRecord(registry, DiagnosticModule::NTP);
  TEST_ASSERT_EQUAL_UINT32(100, record.lastChangeMillis);
  TEST_ASSERT_EQUAL_UINT16(1, record.counters.stateChanges);
}

static void test_reason_change_is_a_diagnostic_transition() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::WIFI,
                                    DiagnosticState::STATE_ERROR,
                                    DiagnosticReason::TIMEOUT, 30));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::WIFI,
                                    DiagnosticState::STATE_ERROR,
                                    DiagnosticReason::SERVICE_UNAVAILABLE, 40));
  const DiagnosticRecord record = readRecord(registry, DiagnosticModule::WIFI);
  TEST_ASSERT_EQUAL_UINT32(40, record.lastChangeMillis);
  TEST_ASSERT_EQUAL_UINT16(2, record.counters.stateChanges);
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::SERVICE_UNAVAILABLE),
                    static_cast<int>(record.reason));
}

static void test_counters_are_independent_and_saturating() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.recordSuccess(DiagnosticModule::PPS, 12));
  TEST_ASSERT_TRUE(registry.recordFailure(DiagnosticModule::PPS, 3));
  TEST_ASSERT_TRUE(registry.recordSuccess(DiagnosticModule::PPS, UINT16_MAX));
  TEST_ASSERT_TRUE(registry.recordFailure(DiagnosticModule::PPS, UINT16_MAX));
  const DiagnosticRecord pps = readRecord(registry, DiagnosticModule::PPS);
  const DiagnosticRecord gps = readRecord(registry, DiagnosticModule::GPS);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, pps.counters.successes);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, pps.counters.failures);
  TEST_ASSERT_EQUAL_UINT16(0, gps.counters.successes);
  TEST_ASSERT_EQUAL_UINT16(0, gps.counters.failures);
}

static void test_snapshot_is_a_compact_independent_copy() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::SI5351A,
                                    DiagnosticState::STATE_OPERATIONAL,
                                    DiagnosticReason::NONE, 77));
  DiagnosticSnapshot snapshot;
  registry.snapshot(snapshot);
  TEST_ASSERT_EQUAL_UINT32(77, snapshot.modules[2].lastChangeMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_OPERATIONAL),
                    static_cast<int>(snapshot.modules[2].state));
  TEST_ASSERT_EQUAL_UINT32(72, sizeof(snapshot));

  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::SI5351A,
                                    DiagnosticState::STATE_DEGRADED,
                                    DiagnosticReason::INVALID_DATA, 88));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_OPERATIONAL),
                    static_cast<int>(snapshot.modules[2].state));
}

static void test_invalid_module_is_rejected() {
  DiagnosticRegistry registry;
  DiagnosticRecord record;
  const DiagnosticModule invalid = static_cast<DiagnosticModule>(99);
  TEST_ASSERT_FALSE(registry.setState(invalid, DiagnosticState::STATE_ERROR,
                                     DiagnosticReason::INTERNAL_ERROR, 1));
  TEST_ASSERT_FALSE(registry.recordSuccess(invalid));
  TEST_ASSERT_FALSE(registry.recordFailure(invalid));
  TEST_ASSERT_FALSE(registry.read(invalid, record));
}

static void test_runtime_diagnostic_sequences() {
  DiagnosticRegistry registry;
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS,
                                    DiagnosticState::STATE_INITIALIZING,
                                    DiagnosticReason::WAITING_FOR_DATA, 10));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS,
                                    DiagnosticState::STATE_OPERATIONAL,
                                    DiagnosticReason::NONE, 20));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::GPS,
                                    DiagnosticState::STATE_DEGRADED,
                                    DiagnosticReason::TIMEOUT, 5021));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::PPS,
                                    DiagnosticState::STATE_INITIALIZING,
                                    DiagnosticReason::WAITING_FOR_DATA, 10));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::PPS,
                                    DiagnosticState::STATE_OPERATIONAL,
                                    DiagnosticReason::NONE, 100));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::PPS,
                                    DiagnosticState::STATE_DEGRADED,
                                    DiagnosticReason::TIMEOUT, 2601));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::WIFI,
                                    DiagnosticState::STATE_DISABLED,
                                    DiagnosticReason::DISABLED_BY_CONFIGURATION, 10));
  TEST_ASSERT_TRUE(registry.setState(DiagnosticModule::NTP,
                                    DiagnosticState::STATE_DISABLED,
                                    DiagnosticReason::NOT_IMPLEMENTED, 10));

  const DiagnosticRecord gps = readRecord(registry, DiagnosticModule::GPS);
  const DiagnosticRecord pps = readRecord(registry, DiagnosticModule::PPS);
  const DiagnosticRecord wifi = readRecord(registry, DiagnosticModule::WIFI);
  const DiagnosticRecord ntp = readRecord(registry, DiagnosticModule::NTP);
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_DEGRADED),
                    static_cast<int>(gps.state));
  TEST_ASSERT_EQUAL_UINT16(3, gps.counters.stateChanges);
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::TIMEOUT),
                    static_cast<int>(pps.reason));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticState::STATE_DISABLED),
                    static_cast<int>(wifi.state));
  TEST_ASSERT_EQUAL(static_cast<int>(DiagnosticReason::NOT_IMPLEMENTED),
                    static_cast<int>(ntp.reason));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_is_unknown_and_not_evaluated);
  RUN_TEST(test_all_modules_and_states_are_representable);
  RUN_TEST(test_unknown_is_distinct_from_not_detected);
  RUN_TEST(test_state_transition_updates_reason_timestamp_and_counter);
  RUN_TEST(test_repeated_state_does_not_create_a_transition);
  RUN_TEST(test_reason_change_is_a_diagnostic_transition);
  RUN_TEST(test_counters_are_independent_and_saturating);
  RUN_TEST(test_snapshot_is_a_compact_independent_copy);
  RUN_TEST(test_invalid_module_is_rejected);
  RUN_TEST(test_runtime_diagnostic_sequences);
  return UNITY_END();
}
