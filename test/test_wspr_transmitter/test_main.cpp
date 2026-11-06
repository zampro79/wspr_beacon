#include <unity.h>

#include "WsprTransmitter.h"
#include "ManualTxController.h"

class FakeRfDriver : public RfDriver {
 public:
  bool operational = true;
  bool frequencyResult = true;
  bool outputEnabled = false;
  uint32_t frequencyCalls = 0;
  uint32_t enableCalls = 0;
  uint32_t disableCalls = 0;
  uint64_t lastFrequency = 0;

  bool detected() override { return operational; }
  bool initialize(uint32_t, int32_t) override { return operational; }
  bool setFrequency(uint64_t frequency) override {
    ++frequencyCalls;
    lastFrequency = frequency;
    return operational && frequencyResult;
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

TimeSnapshot authorizedTime(uint32_t utcMillis) {
  TimeSnapshot time;
  time.source = TimeSource::GPS;
  time.quality = TimeQuality::PRECISE;
  time.utcMillisOfDay = utcMillis;
  time.statusFlags = (1U << 0) | (1U << 1) | (1U << 3);
  return time;
}
TimeSnapshot authorizedNtpTime(uint32_t utcMillis) {
  TimeSnapshot time;
  time.source = TimeSource::NTP;
  time.quality = TimeQuality::SYNCHRONIZED;
  time.utcMillisOfDay = utcMillis;
  time.statusFlags = (1U << 0) | (1U << 1);
  return time;
}
LocatorSnapshot location(const char *locator) {
  LocatorSnapshot result;
  strncpy(result.locator, locator, sizeof(result.locator) - 1U);
  result.source = LocatorSource::GPS_LIVE;
  return result;
}
BeaconConfig configuredConfig() {
  BeaconConfig config;
  strcpy(config.callsign, "IW2NNV");
  return config;
}
void test_idle_preparing_tx_and_first_symbol_keep_original_deadline() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  encoder.firstSymbol = 3;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());

  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(1, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.frequencyCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  uint32_t margin = 0;
  TEST_ASSERT_TRUE(transmitter.preparationMarginMillis(5000UL, margin));
  TEST_ASSERT_EQUAL_UINT32(5000UL, margin);

  TEST_ASSERT_TRUE(transmitter.releasePreparation());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(1, driver.frequencyCalls);
  TEST_ASSERT_EQUAL_UINT64(1409710438ULL, driver.lastFrequency);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);

  transmitter.update(authorizedTime(240000UL), location("RR99"), 10000UL,
                     777000UL);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransmitterState::TRANSMITTING),
      static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(1, driver.enableCalls);
  TEST_ASSERT_TRUE(driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT32(1, driver.frequencyCalls);

  transmitter.update(authorizedTime(240000UL), location("RR99"), 10000UL,
                     5683666UL);
  TEST_ASSERT_EQUAL_UINT32(1, driver.frequencyCalls);
  transmitter.update(authorizedTime(240000UL), location("RR99"), 10000UL,
                     5683667UL);
  TEST_ASSERT_EQUAL_UINT32(2, driver.frequencyCalls);
  TEST_ASSERT_EQUAL_UINT64(1409710000ULL, driver.lastFrequency);
}

void test_prepared_message_is_immutable_until_tx() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  strcpy(config.callsign, "ABCDEF");
  config.powerDbm = 60;
  TEST_ASSERT_EQUAL_STRING("IW2NNV",
                           transmitter.preparedSnapshot().message.callsign);
  TEST_ASSERT_EQUAL_STRING("JN45",
                           transmitter.preparedSnapshot().message.locator);
  TEST_ASSERT_EQUAL_UINT8(10,
                          transmitter.preparedSnapshot().message.powerDbm);
  TEST_ASSERT_EQUAL_STRING("IW2NNV", encoder.lastCallsign);
}

void test_preparing_fail_safe_advances_without_oled_acknowledgement() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  transmitter.update(authorizedTime(235001UL), location("JN45"), 5001UL,
                     2000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(1, driver.frequencyCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_abort_invalidates_prepared_snapshot_and_disables_rf() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  transmitter.reportCriticalRfError();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
      static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_FALSE(transmitter.preparedSnapshot().valid);
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_abort_during_preparing_and_tx_disables_output() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver preparingDriver;
  RfSafetyController preparingRf(preparingDriver, config.si5351ReferenceHz,
                              config.si5351Correction);
  JTEncode preparingEncoder;
  WsprTransmitter preparing(config, preparingRf, preparingEncoder);
  TEST_ASSERT_TRUE(preparing.begin());
  preparing.update(authorizedTime(235000UL), location("JN45"), 5000UL, 1000UL);
  TEST_ASSERT_TRUE(preparing.releasePreparation());
  preparing.reportCriticalRfError();
  TEST_ASSERT_FALSE(preparingDriver.outputEnabled);
  TEST_ASSERT_FALSE(preparing.preparedSnapshot().valid);

  FakeRfDriver txDriver;
  RfSafetyController txRf(txDriver, config.si5351ReferenceHz,
                          config.si5351Correction);
  JTEncode txEncoder;
  WsprTransmitter transmitting(config, txRf, txEncoder);
  TEST_ASSERT_TRUE(transmitting.begin());
  transmitting.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                      1000UL);
  TEST_ASSERT_TRUE(transmitting.releasePreparation());
  transmitting.update(authorizedTime(240000UL), location("JN45"), 10000UL,
                      2000UL);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransmitterState::TRANSMITTING),
      static_cast<uint8_t>(transmitting.state()));
  transmitting.reportCriticalRfError();
  TEST_ASSERT_FALSE(txDriver.outputEnabled);
  TEST_ASSERT_FALSE(transmitting.preparedSnapshot().valid);
}

void test_frozen_authorization_still_starts_at_armed_slot() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_TRUE(transmitter.releasePreparation());

  TimeSnapshot unavailable;
  transmitter.update(unavailable, location("JN45"), 10000UL, 5001000UL);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransmitterState::TRANSMITTING),
      static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(1, driver.enableCalls);
}

void test_missed_slot_never_enables_prepared_output() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_TRUE(transmitter.releasePreparation());
  transmitter.update(authorizedTime(242000UL), location("JN45"), 12000UL,
                     7001000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::IDLE),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::SLOT_MISSED),
      static_cast<uint8_t>(transmitter.telemetry().inhibitReason));
}

void test_frequency_error_during_preparing_fails_safe() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  driver.frequencyResult = false;
  TEST_ASSERT_FALSE(transmitter.releasePreparation());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
      static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(transmitter.preparedSnapshot().valid);
}

void test_fixed_is_default_and_random_mode_freezes_one_frequency_per_frame() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  encoder.firstSymbol = 3;
  WsprTransmitter transmitter(config, rf, encoder);
  transmitter.seedFrequencySelector(0x12345678UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(transmitter.frequencyMode()));
  TEST_ASSERT_TRUE(transmitter.begin());
  TEST_ASSERT_TRUE(transmitter.setFrequencyMode(WsprFrequencyMode::RANDOM));

  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  const TxPreparedSnapshot prepared = transmitter.preparedSnapshot();
  TEST_ASSERT_TRUE(prepared.valid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
                          static_cast<uint8_t>(prepared.band));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(prepared.frequencyMode));
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(14097010UL, prepared.frequencyHz);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(14097189UL, prepared.frequencyHz);
  for (uint8_t update = 0; update < 5; ++update) {
    transmitter.update(authorizedTime(235001UL + update), location("JN45"),
                       5001UL + update, 2000UL + update);
    TEST_ASSERT_EQUAL_UINT32(prepared.frequencyHz,
                             transmitter.preparedSnapshot().frequencyHz);
  }
  TEST_ASSERT_FALSE(transmitter.setFrequencyMode(WsprFrequencyMode::FIXED));
  TEST_ASSERT_TRUE(transmitter.releasePreparation());
  TEST_ASSERT_EQUAL_UINT64(prepared.frequencyHz * 100ULL + 438ULL,
                           driver.lastFrequency);

  transmitter.update(authorizedTime(240000UL), location("RR99"), 10000UL,
                     777000UL);
  TEST_ASSERT_FALSE(transmitter.setFrequencyMode(WsprFrequencyMode::FIXED));
  transmitter.update(authorizedTime(240000UL), location("RR99"), 10000UL,
                     5683667UL);
  TEST_ASSERT_EQUAL_UINT64(prepared.frequencyHz * 100ULL,
                           driver.lastFrequency);
  TEST_ASSERT_EQUAL_UINT32(prepared.frequencyHz,
                           transmitter.preparedSnapshot().frequencyHz);
}

void test_unconfigured_identity_blocks_all_preparation_and_encoding() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  BeaconSettings unconfigured;
  unconfigured.callsign[0] = '\0';
  unconfigured.frequencyMode = WsprFrequencyMode::RANDOM;

  TEST_ASSERT_TRUE(transmitter.setOperatingIdentityUnconfigured(unconfigured));
  TEST_ASSERT_TRUE(transmitter.begin());
  TEST_ASSERT_FALSE(transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(
          OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(transmitter.operatingIdentityState()));

  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(
          TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(transmitter.telemetry().inhibitReason));
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.frequencyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(transmitter.preparedSnapshot().valid);

  transmitter.update(authorizedNtpTime(355000UL), location("JN45"), 125000UL,
                     2000UL);
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.frequencyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);

  TEST_ASSERT_TRUE(transmitter.recoverRf());
  TEST_ASSERT_FALSE(transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(
          TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(transmitter.telemetry().inhibitReason));
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
}

void test_profile_default_identity_initializes_transmitter_safely() {
  BeaconConfig config;
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  TEST_ASSERT_TRUE(transmitter.begin());
#if defined(WSPR_PROFILE_PUBLIC)
  TEST_ASSERT_FALSE(transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(transmitter.operatingIdentityState()));
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
#else
  TEST_ASSERT_TRUE(transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperatingIdentityState::CONFIGURED),
      static_cast<uint8_t>(transmitter.operatingIdentityState()));
#endif
}

void test_valid_identity_restores_eligibility_without_immediate_tx() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  BeaconSettings unconfigured;
  unconfigured.callsign[0] = '\0';
  TEST_ASSERT_TRUE(transmitter.setOperatingIdentityUnconfigured(unconfigured));
  TEST_ASSERT_TRUE(transmitter.begin());

  BeaconSettings configured;
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(transmitter.applySettings(configured));
  TEST_ASSERT_TRUE(transmitter.txAdmissionAllowed());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(OperatingIdentityState::CONFIGURED),
      static_cast<uint8_t>(transmitter.operatingIdentityState()));
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_manual_control_defaults_on_reboots_on_and_commands_are_idempotent() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  ManualTxController manual(transmitter);
  TEST_ASSERT_TRUE(manual.permitted());
  ManualTxController rebooted(transmitter);
  TEST_ASSERT_TRUE(rebooted.permitted());
  TEST_ASSERT_TRUE(transmitter.begin());
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
  TEST_ASSERT_TRUE(manual.turnOff());
  TEST_ASSERT_TRUE(manual.turnOff());
  TEST_ASSERT_FALSE(manual.permitted());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::MANUAL_TX_OFF),
      static_cast<uint8_t>(transmitter.telemetry().inhibitReason));
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(manual.turnOn(false));
  TEST_ASSERT_FALSE(manual.permitted());
  TEST_ASSERT_TRUE(manual.turnOn(true));
  TEST_ASSERT_TRUE(manual.turnOn(true));
  TEST_ASSERT_TRUE(manual.permitted());
  transmitter.setTxAdmissionAllowed(true);
  TEST_ASSERT_EQUAL_UINT32(0, encoder.encodeCalls);
  TEST_ASSERT_EQUAL_UINT32(0, driver.enableCalls);
}

void test_manual_off_aborts_active_frame_without_rf_fault() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  ManualTxController manual(transmitter);
  TEST_ASSERT_TRUE(transmitter.begin());
  TEST_ASSERT_TRUE(manual.turnOn(true));
  transmitter.setTxAdmissionAllowed(true);
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_TRUE(transmitter.releasePreparation());
  transmitter.update(authorizedTime(240000UL), location("JN45"), 10000UL,
                     5001000UL);
  TEST_ASSERT_TRUE(driver.outputEnabled);
  manual.turnOff();
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxFrameResult::INTERRUPTED_MANUAL_TX_OFF),
      static_cast<uint8_t>(transmitter.telemetry().lastFrame.result));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::MANUAL_TX_OFF),
      static_cast<uint8_t>(transmitter.telemetry().lastFrame.endReason));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RfFault::NONE),
                          static_cast<uint8_t>(rf.fault()));
}

void test_manual_off_cancels_preparing_without_rf_fault() {
  BeaconConfig config = configuredConfig();
  FakeRfDriver driver;
  RfSafetyController rf(driver, config.si5351ReferenceHz,
                         config.si5351Correction);
  JTEncode encoder;
  WsprTransmitter transmitter(config, rf, encoder);
  ManualTxController manual(transmitter);
  TEST_ASSERT_TRUE(transmitter.begin());
  TEST_ASSERT_TRUE(manual.turnOn(true));
  transmitter.setTxAdmissionAllowed(true);
  transmitter.update(authorizedTime(235000UL), location("JN45"), 5000UL,
                     1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_TRUE(transmitter.preparedSnapshot().valid);
  TEST_ASSERT_TRUE(manual.turnOff());
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(transmitter.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::MANUAL_TX_OFF),
                          static_cast<uint8_t>(transmitter.telemetry().inhibitReason));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RfFault::NONE),
                          static_cast<uint8_t>(rf.fault()));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_preparing_tx_and_first_symbol_keep_original_deadline);
  RUN_TEST(test_prepared_message_is_immutable_until_tx);
  RUN_TEST(test_preparing_fail_safe_advances_without_oled_acknowledgement);
  RUN_TEST(test_abort_invalidates_prepared_snapshot_and_disables_rf);
  RUN_TEST(test_abort_during_preparing_and_tx_disables_output);
  RUN_TEST(test_frozen_authorization_still_starts_at_armed_slot);
  RUN_TEST(test_missed_slot_never_enables_prepared_output);
  RUN_TEST(test_frequency_error_during_preparing_fails_safe);
  RUN_TEST(test_fixed_is_default_and_random_mode_freezes_one_frequency_per_frame);
  RUN_TEST(test_unconfigured_identity_blocks_all_preparation_and_encoding);
  RUN_TEST(test_profile_default_identity_initializes_transmitter_safely);
  RUN_TEST(test_valid_identity_restores_eligibility_without_immediate_tx);
  RUN_TEST(test_manual_control_defaults_on_reboots_on_and_commands_are_idempotent);
  RUN_TEST(test_manual_off_aborts_active_frame_without_rf_fault);
  RUN_TEST(test_manual_off_cancels_preparing_without_rf_fault);
  return UNITY_END();
}
