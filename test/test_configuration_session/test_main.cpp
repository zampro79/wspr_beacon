#include <unity.h>

#include "CommandLineBuffer.h"
#include "ConfigCommandParser.h"
#include "ConfigurationSessionController.h"

namespace {
constexpr char OWNER_A[] = "00112233445566778899aabbccddeeff";
constexpr char OWNER_B[] = "ffeeddccbbaa99887766554433221100";

class FakeRfDriver : public RfDriver {
 public:
  bool operational = true;
  bool outputEnabled = false;
  uint32_t frequencyCalls = 0;
  uint32_t enableCalls = 0;
  uint32_t disableCalls = 0;

  bool detected() override { return operational; }
  bool initialize(uint32_t, int32_t) override { return operational; }
  bool setFrequency(uint64_t) override {
    ++frequencyCalls;
    return operational;
  }
  void enableOutput() override {
    ++enableCalls;
    outputEnabled = true;
  }
  void disableOutput() override {
    ++disableCalls;
    outputEnabled = false;
  }
};

struct Fixture {
  BeaconConfig config;
  FakeRfDriver driver;
  RfSafetyController rf;
  JTEncode encoder;
  WsprTransmitter transmitter;
  ConfigurationSessionController sessions;

  Fixture()
      : rf(driver, config.si5351ReferenceHz, config.si5351Correction),
        transmitter(config, rf, encoder) {
    BeaconSettings configured;
    strcpy(configured.callsign, "IW2NNV");
    TEST_ASSERT_TRUE(transmitter.applySettings(configured));
    TEST_ASSERT_TRUE(transmitter.begin());
  }
};

TimeSnapshot authorizedTime(uint32_t utcMillis) {
  TimeSnapshot time;
  time.source = TimeSource::GPS;
  time.quality = TimeQuality::PRECISE;
  time.utcMillisOfDay = utcMillis;
  time.statusFlags = (1U << 0) | (1U << 1) | (1U << 3);
  return time;
}

LocatorSnapshot location() {
  LocatorSnapshot result;
  strcpy(result.locator, "JN45");
  result.source = LocatorSource::GPS_LIVE;
  return result;
}

CommandResult parse(ConfigCommandParser &parser, const char *command) {
  char line[CommandLineBuffer::CAPACITY];
  strncpy(line, command, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  return parser.execute(line);
}
}  // namespace

void test_normal_startup_and_identifier_validation() {
  Fixture fixture;
  const ConfigurationSessionSnapshot snapshot = fixture.sessions.snapshot(0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FirmwareOperatingMode::NORMAL),
                          static_cast<uint8_t>(snapshot.mode));
  TEST_ASSERT_TRUE(snapshot.txAdmissionAllowed);
  TEST_ASSERT_FALSE(snapshot.cliActive);
  TEST_ASSERT_FALSE(snapshot.webActive);
  TEST_ASSERT_TRUE(ConfigurationSessionController::validWebSessionId(OWNER_A));
  TEST_ASSERT_FALSE(ConfigurationSessionController::validWebSessionId("short"));
  TEST_ASSERT_FALSE(ConfigurationSessionController::validWebSessionId(
      "00112233445566778899AABBCCDDEEFF"));
}

void test_cli_acquire_release_is_idempotent_and_starts_resume_guard() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  TEST_ASSERT_FALSE(fixture.transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT32(1, fixture.sessions.revision());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::UNCHANGED),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT32(1, fixture.sessions.revision());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.releaseCli(fixture.transmitter)));
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(fixture.transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT32(2, fixture.sessions.revision());
}

void test_web_owner_renewal_expiry_and_wrong_owner() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireWeb(
          OWNER_A, UINT32_MAX - 9999UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::OWNER_MISMATCH),
      static_cast<uint8_t>(fixture.sessions.acquireWeb(
          OWNER_B, UINT32_MAX - 5000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.renewWeb(
          OWNER_A, UINT32_MAX - 5000UL, fixture.transmitter)));
  TEST_ASSERT_FALSE(fixture.sessions.expireWeb(19998UL, fixture.transmitter));
  TEST_ASSERT_TRUE(fixture.sessions.expireWeb(19999UL, fixture.transmitter));
  TEST_ASSERT_FALSE(fixture.sessions.webActive());
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::NOT_ACTIVE),
      static_cast<uint8_t>(fixture.sessions.renewWeb(
          OWNER_A, 20000UL, fixture.transmitter)));
}

void test_web_mutation_authorization_requires_live_matching_owner() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireWeb(
          OWNER_A, 1000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.authorizeWebMutation(
          OWNER_A, 2000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::OWNER_MISMATCH),
      static_cast<uint8_t>(fixture.sessions.authorizeWebMutation(
          nullptr, 2000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::OWNER_MISMATCH),
      static_cast<uint8_t>(fixture.sessions.authorizeWebMutation(
          OWNER_B, 2000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionResult::NOT_ACTIVE),
      static_cast<uint8_t>(fixture.sessions.authorizeWebMutation(
          OWNER_A, 26000UL, fixture.transmitter)));
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
}

void test_concurrent_owners_release_independently() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireWeb(
          OWNER_A, 1000UL, fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.releaseCli(fixture.transmitter)));
  TEST_ASSERT_TRUE(fixture.sessions.webActive());
  TEST_ASSERT_FALSE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.releaseWeb(
          OWNER_A, fixture.transmitter)));
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
}

void test_acquisition_rejected_during_preparing_and_transmitting() {
  Fixture fixture;
  fixture.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(fixture.transmitter.state()));
  const uint32_t revisionBeforePreparingReject = fixture.sessions.revision();
  const uint32_t disablesBeforePreparingReject = fixture.driver.disableCalls;
  uint32_t marginBeforeReject = 0;
  TEST_ASSERT_TRUE(fixture.transmitter.preparationMarginMillis(
      5000UL, marginBeforeReject));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::RF_BUSY),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  TEST_ASSERT_FALSE(fixture.sessions.cliActive());
  TEST_ASSERT_FALSE(fixture.sessions.webActive());
  TEST_ASSERT_EQUAL_UINT32(revisionBeforePreparingReject,
                           fixture.sessions.revision());
  TEST_ASSERT_EQUAL_UINT32(disablesBeforePreparingReject,
                           fixture.driver.disableCalls);
  uint32_t marginAfterReject = 0;
  TEST_ASSERT_TRUE(fixture.transmitter.preparationMarginMillis(
      5000UL, marginAfterReject));
  TEST_ASSERT_EQUAL_UINT32(marginBeforeReject, marginAfterReject);
  TEST_ASSERT_TRUE(fixture.transmitter.releasePreparation());
  fixture.transmitter.update(authorizedTime(240000UL), location(), 10000UL,
                             5001000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::TRANSMITTING),
                          static_cast<uint8_t>(fixture.transmitter.state()));
  const uint32_t revisionBeforeTxReject = fixture.sessions.revision();
  const uint32_t disablesBeforeTxReject = fixture.driver.disableCalls;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::RF_BUSY),
      static_cast<uint8_t>(fixture.sessions.acquireWeb(
          OWNER_A, 10000UL, fixture.transmitter)));
  TEST_ASSERT_FALSE(fixture.sessions.cliActive());
  TEST_ASSERT_FALSE(fixture.sessions.webActive());
  TEST_ASSERT_EQUAL_UINT32(revisionBeforeTxReject,
                           fixture.sessions.revision());
  TEST_ASSERT_EQUAL_UINT32(disablesBeforeTxReject,
                           fixture.driver.disableCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::TRANSMITTING),
                          static_cast<uint8_t>(fixture.transmitter.state()));
}

void test_acquisition_allowed_from_idle_and_rf_inhibited_when_output_is_off() {
  Fixture idle;
  idle.transmitter.update(authorizedTime(200000UL), location(), 1000UL,
                          1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::IDLE),
                          static_cast<uint8_t>(idle.transmitter.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(idle.sessions.acquireCli(idle.transmitter)));

  Fixture inhibited;
  inhibited.transmitter.reportCriticalRfError();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(inhibited.transmitter.state()));
  TEST_ASSERT_FALSE(inhibited.transmitter.rfOutputEnabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(inhibited.sessions.acquireWeb(
          OWNER_A, 1000UL, inhibited.transmitter)));
}

void test_no_preparation_random_selection_or_telemetry_while_active() {
  Fixture fixture;
  fixture.transmitter.seedFrequencySelector(0x12345678UL);
  TEST_ASSERT_TRUE(fixture.transmitter.setFrequencyMode(WsprFrequencyMode::RANDOM));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  fixture.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  fixture.transmitter.update(authorizedTime(240000UL), location(), 10000UL,
                             5001000UL);
  fixture.transmitter.update(authorizedTime(475000UL), location(), 245000UL,
                             9000000UL);
  TEST_ASSERT_FALSE(fixture.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.driver.frequencyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.transmitter.telemetry().framesStarted);
}

void test_resume_guard_requires_safe_qualified_future_slot() {
  Fixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  fixture.sessions.releaseCli(fixture.transmitter);
  fixture.sessions.updateTxAdmission(false, true, true, 30000UL,
                                     fixture.transmitter);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  fixture.sessions.updateTxAdmission(true, false, true, 30000UL,
                                     fixture.transmitter);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  fixture.sessions.updateTxAdmission(true, true, true, 7000UL,
                                     fixture.transmitter);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  fixture.sessions.updateTxAdmission(true, true, true, 7001UL,
                                     fixture.transmitter);
  TEST_ASSERT_FALSE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_TRUE(fixture.transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT32(3, fixture.sessions.revision());
}

void test_explicit_transition_resume_guard_blocks_immediate_preparation() {
  Fixture fixture;
  fixture.sessions.activateResumeGuard(fixture.transmitter);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(fixture.transmitter.txAdmissionAllowed());
  fixture.sessions.updateTxAdmission(true, true, true, 5000UL,
                                     fixture.transmitter);
  fixture.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(fixture.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.encoder.encodeCalls);
  fixture.sessions.updateTxAdmission(true, true, true, 115000UL,
                                     fixture.transmitter);
  TEST_ASSERT_FALSE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_TRUE(fixture.transmitter.txAdmissionAllowed());
}

void test_release_near_slot_skips_it_and_resumes_without_catch_up() {
  Fixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  fixture.sessions.releaseCli(fixture.transmitter);

  fixture.sessions.updateTxAdmission(true, true, true, 5000UL,
                                     fixture.transmitter);
  fixture.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(fixture.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.encoder.encodeCalls);

  fixture.sessions.updateTxAdmission(true, true, true, 115000UL,
                                     fixture.transmitter);
  fixture.transmitter.update(authorizedTime(245000UL), location(), 15000UL,
                             10001000UL);
  TEST_ASSERT_FALSE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_TRUE(fixture.transmitter.txAdmissionAllowed());
  TEST_ASSERT_FALSE(fixture.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.transmitter.telemetry().framesStarted);
}

void test_operation_guard_survives_web_expiry() {
  Fixture fixture;
  fixture.sessions.acquireWeb(OWNER_A, 0, fixture.transmitter);
  {
    ConfigurationOperationGuard guard(fixture.sessions, fixture.transmitter);
    TEST_ASSERT_TRUE(guard.active());
    TEST_ASSERT_TRUE(fixture.sessions.expireWeb(25000UL, fixture.transmitter));
    TEST_ASSERT_TRUE(fixture.sessions.operationActive());
    TEST_ASSERT_FALSE(fixture.transmitter.txAdmissionAllowed());
  }
  TEST_ASSERT_FALSE(fixture.sessions.operationActive());
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(fixture.transmitter.txAdmissionAllowed());
}

void test_cli_exit_retains_candidate_without_applying_or_saving() {
  Fixture fixture;
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(parse(parser, "wspr set power 13").status));
  TEST_ASSERT_EQUAL_UINT8(13, parser.candidate().powerDbm);
  TEST_ASSERT_EQUAL_UINT8(10, fixture.config.powerDbm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.releaseCli(fixture.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(13, parser.candidate().powerDbm);
  TEST_ASSERT_EQUAL_UINT8(10, fixture.config.powerDbm);
  TEST_ASSERT_TRUE(fixture.sessions.resumeGuardActive());
}

void test_defensive_start_check_never_enables_rf() {
  Fixture fixture;
  fixture.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_TRUE(fixture.transmitter.releasePreparation());
  fixture.transmitter.setTxAdmissionAllowed(false);
  fixture.transmitter.update(authorizedTime(240000UL), location(), 10000UL,
                             5001000UL);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.driver.enableCalls);
  TEST_ASSERT_FALSE(fixture.driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(fixture.transmitter.state()));
}

void test_revision_wrap_helper_and_reboot_equivalent_defaults() {
  TEST_ASSERT_EQUAL_UINT32(0,
      ConfigurationSessionController::nextRevision(UINT32_MAX));
  ConfigurationSessionController fresh;
  const ConfigurationSessionSnapshot snapshot = fresh.snapshot(1234UL);
  TEST_ASSERT_EQUAL_UINT32(0, snapshot.revision);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FirmwareOperatingMode::NORMAL),
                          static_cast<uint8_t>(snapshot.mode));
  TEST_ASSERT_TRUE(snapshot.txAdmissionAllowed);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_normal_startup_and_identifier_validation);
  RUN_TEST(test_cli_acquire_release_is_idempotent_and_starts_resume_guard);
  RUN_TEST(test_web_owner_renewal_expiry_and_wrong_owner);
  RUN_TEST(test_web_mutation_authorization_requires_live_matching_owner);
  RUN_TEST(test_concurrent_owners_release_independently);
  RUN_TEST(test_acquisition_rejected_during_preparing_and_transmitting);
  RUN_TEST(test_acquisition_allowed_from_idle_and_rf_inhibited_when_output_is_off);
  RUN_TEST(test_no_preparation_random_selection_or_telemetry_while_active);
  RUN_TEST(test_resume_guard_requires_safe_qualified_future_slot);
  RUN_TEST(test_explicit_transition_resume_guard_blocks_immediate_preparation);
  RUN_TEST(test_release_near_slot_skips_it_and_resumes_without_catch_up);
  RUN_TEST(test_operation_guard_survives_web_expiry);
  RUN_TEST(test_cli_exit_retains_candidate_without_applying_or_saving);
  RUN_TEST(test_defensive_start_check_never_enables_rf);
  RUN_TEST(test_revision_wrap_helper_and_reboot_equivalent_defaults);
  return UNITY_END();
}
