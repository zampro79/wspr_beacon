#include <unity.h>

#include <string.h>

#include "ConfigCommandParser.h"
#include "ConfigurationService.h"
#include "ConfigurationSaveJson.h"
#include "PersistentConfigRecord.h"

namespace {
class FakeStorage : public ConfigStorage {
 public:
  bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
            size_t capacity, size_t &length) override {
    const uint8_t typeIndex = static_cast<uint8_t>(type) - 1U;
    if (typeIndex > 1 || slot > 1 || lengths[typeIndex][slot] == 0 ||
        capacity < lengths[typeIndex][slot]) {
      length = 0;
      return false;
    }
    if (failVerificationRead && writes[typeIndex] > 0) {
      length = 0;
      return false;
    }
    length = lengths[typeIndex][slot];
    memcpy(data, records[typeIndex][slot], length);
    return true;
  }

  bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) override {
    const uint8_t typeIndex = static_cast<uint8_t>(type) - 1U;
    ++writes[typeIndex];
    if (failType == type) return false;
    if (typeIndex > 1 || slot > 1 ||
        length > PersistentConfigRecord::MAX_RECORD_SIZE) {
      return false;
    }
    memcpy(records[typeIndex][slot], data, length);
    lengths[typeIndex][slot] = length;
    return true;
  }

  uint8_t records[2][2][PersistentConfigRecord::MAX_RECORD_SIZE] = {};
  size_t lengths[2][2] = {};
  uint16_t writes[2] = {};
  ConfigRecordType failType = static_cast<ConfigRecordType>(0);
  bool failVerificationRead = false;
};

class FakeRfDriver : public RfDriver {
 public:
  bool detected() override { return true; }
  bool initialize(uint32_t, int32_t) override { return true; }
  bool setFrequency(uint64_t) override { return true; }
  void enableOutput() override { outputEnabled = true; }
  void disableOutput() override { outputEnabled = false; }
  bool outputEnabled = false;
};

class FakeRuntime : public ConfigurationRuntime {
 public:
  bool busy() const override { return busyState; }
  bool applyWspr(const BeaconSettings &settings) override {
    ++applyCalls;
    applied = settings;
    if (failedAppliesRemaining > 0) {
      --failedAppliesRemaining;
      return false;
    }
    return true;
  }
  void applyPowerSave(bool enabled) override {
    ++powerSaveCalls;
    appliedPowerSave = enabled;
  }
  bool selectFrequencyMode(WsprFrequencyMode mode) override {
    ++modeCalls;
    if (busyState || failMode) return false;
    selectedMode = mode;
    return true;
  }

  bool busyState = false;
  uint8_t failedAppliesRemaining = 0;
  bool failMode = false;
  uint8_t applyCalls = 0;
  uint8_t modeCalls = 0;
  uint8_t powerSaveCalls = 0;
  bool appliedPowerSave = false;
  BeaconSettings applied;
  WsprFrequencyMode selectedMode = WsprFrequencyMode::FIXED;
};

BeaconSettings callsign(const BeaconSettings &base, const char *value) {
  BeaconSettings result = base;
  strcpy(result.callsign, value);
  return result;
}

BeaconSettings configuredDefaults() {
  BeaconSettings settings;
  strcpy(settings.callsign, "IW2NNV");
  return settings;
}

WifiCredentials wifi(const char *ssid, const char *password) {
  WifiCredentials result;
  result.setSsid(ssid);
  result.setPassword(password);
  return result;
}

struct SafeSaveFixture {
  FakeStorage storage;
  ConfigurationManager manager;
  FakeRuntime runtime;
  ConfigurationService service;
  BeaconConfig transmitterConfig;
  FakeRfDriver driver;
  RfSafetyController rf;
  JTEncode encoder;
  WsprTransmitter transmitter;
  ConfigurationSessionController sessions;

  SafeSaveFixture()
      : manager(storage, configuredDefaults()), service(manager, runtime),
        rf(driver, transmitterConfig.si5351ReferenceHz,
           transmitterConfig.si5351Correction),
        transmitter(transmitterConfig, rf, encoder) {
    manager.begin();
    TEST_ASSERT_TRUE(transmitter.begin());
  }

  WsprSaveSafetyContext cliContext() {
    return {sessions, transmitter, ConfigurationSaveOwner::CLI, nullptr, 0};
  }
};

CommandResult execute(ConfigCommandParser &parser, const char *command) {
  char line[96];
  strncpy(line, command, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  return parser.execute(line);
}

void test_snapshot_owns_active_candidate_saved_and_revision() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);

  ConfigurationSnapshot initial = service.snapshot();
  TEST_ASSERT_EQUAL_UINT32(0, initial.revision);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, initial.active.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, initial.candidate.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, initial.saved.callsign);

  BeaconSettings changed = callsign(initial.candidate, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(changed, initial.revision)));
  ConfigurationSnapshot staged = service.snapshot();
  TEST_ASSERT_EQUAL_UINT32(1, staged.revision);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", staged.candidate.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, staged.active.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, staged.saved.callsign);
}

void test_complete_candidate_validation_is_atomic() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings invalid = service.snapshot().candidate;
  invalid.frequencyOffsetHz = 101;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::INVALID),
      static_cast<uint8_t>(service.stageWspr(invalid, 0)));
  TEST_ASSERT_EQUAL_INT16(0, service.snapshot().candidate.frequencyOffsetHz);
  TEST_ASSERT_EQUAL_UINT32(0, service.revision());
}

void test_unconfigured_default_can_stage_and_save_valid_identity() {
  FakeStorage storage;
  BeaconSettings unconfigured;
  unconfigured.callsign[0] = '\0';
  ConfigurationManager manager(storage, unconfigured);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(
          OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(manager.activeIdentityState()));
  TEST_ASSERT_EQUAL_STRING("", service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::INVALID),
      static_cast<uint8_t>(service.stageWspr(unconfigured, 0)));

  BeaconSettings configured = unconfigured;
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(configured, 0)));
  TEST_ASSERT_EQUAL_STRING("", service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().candidate.callsign);
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
                          static_cast<uint8_t>(manager.processPending(false)));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.savedWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
}

void test_cli_refresh_preserves_candidate_changes_from_another_interface() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  ConfigCommandParser parser;
  parser.loadCandidates(manager.candidateWspr(), manager.candidateWifi());

  BeaconSettings webCandidate =
      callsign(service.snapshot().candidate, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(webCandidate, 0)));

  parser.setCandidates(service.snapshot().candidate,
                       parser.wifiCredentials());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set locator JN55").status));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(
          service.stageWspr(parser.candidate(), service.revision())));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().candidate.callsign);
  TEST_ASSERT_EQUAL_STRING("JN55", service.snapshot().candidate.fixedLocator);
}

void test_stale_revision_rejects_mutations() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings first = callsign(service.snapshot().candidate, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(first, 0)));
  BeaconSettings stale = callsign(first, "IZ2DEF");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::STALE_REVISION),
      static_cast<uint8_t>(service.stageWspr(stale, 0)));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().candidate.callsign);
}

void test_revision_wraparound_is_defined() {
  TEST_ASSERT_EQUAL_UINT32(0,
      ConfigurationService::nextRevision(UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(1, ConfigurationService::nextRevision(0));
}

void test_discard_restores_saved_without_changing_active() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings changed = callsign(service.snapshot().candidate, "IZ2ABC");
  service.stageWspr(changed, 0);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.discard(1)));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().candidate.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT32(2, service.revision());
}

void test_web_discard_restores_only_wspr_and_preserves_wifi_candidate() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings changed = callsign(service.snapshot().candidate, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(changed, 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWifi(wifi("candidate", "password"), 1)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.discardWspr(2)));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().candidate.callsign);
  TEST_ASSERT_EQUAL_STRING("candidate", manager.candidateWifi().ssid);
  TEST_ASSERT_EQUAL_UINT32(3, service.revision());
}

void test_cli_mode_selection_changes_candidate_and_active() {
  FakeStorage storage;
  ConfigurationManager manager(storage, configuredDefaults());
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  const uint32_t configuredFrequency = service.snapshot().active.baseFrequencyHz;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.selectFrequencyMode(
          WsprFrequencyMode::RANDOM, 0)));
  const ConfigurationSnapshot snapshot = service.snapshot();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(snapshot.candidate.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(snapshot.active.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(snapshot.saved.frequencyMode));
  TEST_ASSERT_TRUE(snapshot.activeDiffersSaved);
  TEST_ASSERT_TRUE(snapshot.status.wsprDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(runtime.selectedMode));
  TEST_ASSERT_EQUAL_UINT32(configuredFrequency,
                           snapshot.active.baseFrequencyHz);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_future_web_mode_staging_is_candidate_only() {
  FakeStorage storage;
  ConfigurationManager manager(storage, configuredDefaults());
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings candidate = service.snapshot().candidate;
  candidate.frequencyMode = WsprFrequencyMode::RANDOM;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(service.stageWspr(candidate, 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
      static_cast<uint8_t>(service.snapshot().candidate.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
      static_cast<uint8_t>(service.snapshot().active.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(0, runtime.modeCalls);
}

void test_save_verifies_persists_applies_and_does_not_write_wifi() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  BeaconSettings changed = callsign(service.snapshot().candidate, "IZ2ABC");
  changed.powerSave = true;
  service.stageWspr(changed, 0);
  const ConfigurationSaveOutcome result = service.save(1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::OK),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
                          static_cast<uint8_t>(result.persistence));
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
  TEST_ASSERT_EQUAL_UINT8(1, runtime.applyCalls);
  TEST_ASSERT_EQUAL_UINT8(1, runtime.powerSaveCalls);
  TEST_ASSERT_TRUE(runtime.appliedPowerSave);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT32(2, service.revision());
}

void test_partial_save_advances_saved_but_not_active() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  service.stageWspr(callsign(service.snapshot().candidate, "IZ2ABC"), 0);
  service.stageWifi(wifi("Network", "password1"), 1);
  storage.failType = ConfigRecordType::WIFI;
  const ConfigurationSaveOutcome result = service.save(2);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::SAVE_PARTIAL),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT8(0, runtime.applyCalls);
  TEST_ASSERT_EQUAL_UINT32(3, service.revision());
}

void test_runtime_apply_failure_is_explicit_and_active_is_rolled_back() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  service.stageWspr(callsign(service.snapshot().candidate, "IZ2ABC"), 0);
  runtime.failedAppliesRemaining = 1;
  const ConfigurationSaveOutcome result = service.save(1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::RUNTIME_APPLY_FAILED),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT8(2, runtime.applyCalls);
  TEST_ASSERT_TRUE(service.snapshot().runtimeSynchronized);
}

void test_failed_runtime_rollback_is_reported_as_unsynchronized() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  service.stageWspr(callsign(service.snapshot().candidate, "IZ2ABC"), 0);
  runtime.failedAppliesRemaining = 2;
  const ConfigurationSaveOutcome result = service.save(1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationOperationResult::RUNTIME_APPLY_FAILED),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_FALSE(service.snapshot().runtimeSynchronized);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           service.snapshot().active.callsign);
}

void test_busy_state_rejects_staging_mode_discard_and_save() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  runtime.busyState = true;
  BeaconSettings changed = callsign(service.snapshot().candidate, "IZ2ABC");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::RF_BUSY),
      static_cast<uint8_t>(service.stageWspr(changed, 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::RF_BUSY),
      static_cast<uint8_t>(service.selectFrequencyMode(
          WsprFrequencyMode::RANDOM, 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::RF_BUSY),
      static_cast<uint8_t>(service.discard(0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationOperationResult::RF_BUSY),
      static_cast<uint8_t>(service.save(0).result));
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_wspr_only_save_requires_owner_and_checks_stale_revision_first() {
  SafeSaveFixture fixture;
  WsprSaveSafetyContext context = fixture.cliContext();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::STALE_REVISION),
      static_cast<uint8_t>(fixture.service.saveWspr(1, context).result));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::CONFIG_MODE_REQUIRED),
      static_cast<uint8_t>(fixture.service.saveWspr(0, context).result));
  TEST_ASSERT_EQUAL_UINT16(0, fixture.storage.writes[0]);
}

void test_wspr_only_save_persists_applies_and_never_writes_wifi() {
  SafeSaveFixture fixture;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(fixture.sessions.acquireCli(fixture.transmitter)));
  fixture.service.stageWifi(wifi("candidate", "password1"), 0);
  fixture.service.stageWspr(
      callsign(fixture.service.snapshot().candidate, "IZ2ABC"), 1);
  WsprSaveSafetyContext context = fixture.cliContext();
  const WsprSaveOutcome result = fixture.service.saveWspr(2, context);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::SAVED_APPLIED),
                          static_cast<uint8_t>(result.result));
  TEST_ASSERT_TRUE(result.persistenceOccurred);
  TEST_ASSERT_TRUE(result.runtimeApplySucceeded);
  TEST_ASSERT_EQUAL_UINT16(1, fixture.storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, fixture.storage.writes[1]);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", fixture.service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", fixture.service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_STRING("candidate", fixture.manager.candidateWifi().ssid);
}

void test_wspr_only_unchanged_performs_no_write_apply_or_revision_change() {
  SafeSaveFixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  WsprSaveSafetyContext context = fixture.cliContext();
  const WsprSaveOutcome result = fixture.service.saveWspr(0, context);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::UNCHANGED),
                          static_cast<uint8_t>(result.result));
  TEST_ASSERT_FALSE(result.persistenceOccurred);
  TEST_ASSERT_FALSE(result.runtimeApplyAttempted);
  TEST_ASSERT_EQUAL_UINT16(0, fixture.storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.runtime.applyCalls);
  TEST_ASSERT_EQUAL_UINT32(0, fixture.service.revision());
}

void test_wspr_only_reports_write_and_verification_failures_truthfully() {
  SafeSaveFixture writeFailure;
  writeFailure.sessions.acquireCli(writeFailure.transmitter);
  writeFailure.service.stageWspr(
      callsign(writeFailure.service.snapshot().candidate, "IZ2ABC"), 0);
  writeFailure.storage.failType = ConfigRecordType::WSPR;
  WsprSaveSafetyContext writeContext = writeFailure.cliContext();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::PERSISTENCE_FAILED),
      static_cast<uint8_t>(writeFailure.service.saveWspr(1, writeContext).result));

  SafeSaveFixture verifyFailure;
  verifyFailure.sessions.acquireCli(verifyFailure.transmitter);
  verifyFailure.service.stageWspr(
      callsign(verifyFailure.service.snapshot().candidate, "IZ2ABC"), 0);
  verifyFailure.storage.failVerificationRead = true;
  WsprSaveSafetyContext verifyContext = verifyFailure.cliContext();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(verifyFailure.service.saveWspr(1, verifyContext).result));
  TEST_ASSERT_EQUAL_STRING("IW2NNV",
                           verifyFailure.service.snapshot().saved.callsign);
}

void test_wspr_only_apply_failure_preserves_saved_and_rolls_back_active() {
  SafeSaveFixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  fixture.service.stageWspr(
      callsign(fixture.service.snapshot().candidate, "IZ2ABC"), 0);
  fixture.runtime.failedAppliesRemaining = 1;
  WsprSaveSafetyContext context = fixture.cliContext();
  const WsprSaveOutcome result = fixture.service.saveWspr(1, context);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::PERSISTED_APPLY_FAILED),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_TRUE(result.persistenceOccurred);
  TEST_ASSERT_TRUE(result.rollbackSucceeded);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", fixture.service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING("IW2NNV", fixture.service.snapshot().active.callsign);
  TEST_ASSERT_TRUE(fixture.sessions.cliActive());
  TEST_ASSERT_EQUAL_UINT32(2, fixture.service.revision());
}

void test_wspr_only_reconciles_saved_active_without_another_flash_write() {
  SafeSaveFixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  fixture.service.stageWspr(
      callsign(fixture.service.snapshot().candidate, "IZ2ABC"), 0);
  fixture.runtime.failedAppliesRemaining = 1;
  WsprSaveSafetyContext context = fixture.cliContext();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::PERSISTED_APPLY_FAILED),
      static_cast<uint8_t>(fixture.service.saveWspr(1, context).result));
  TEST_ASSERT_EQUAL_UINT16(1, fixture.storage.writes[0]);

  const WsprSaveOutcome reconciled = fixture.service.saveWspr(2, context);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::SAVED_APPLIED),
                          static_cast<uint8_t>(reconciled.result));
  TEST_ASSERT_FALSE(reconciled.persistenceOccurred);
  TEST_ASSERT_TRUE(reconciled.runtimeApplySucceeded);
  TEST_ASSERT_EQUAL_UINT16(1, fixture.storage.writes[0]);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", fixture.service.snapshot().active.callsign);
  TEST_ASSERT_EQUAL_UINT32(3, fixture.service.revision());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::STALE_REVISION),
      static_cast<uint8_t>(fixture.service.saveWspr(2, context).result));
}

void test_wspr_only_reports_failed_runtime_rollback() {
  SafeSaveFixture fixture;
  fixture.sessions.acquireCli(fixture.transmitter);
  fixture.service.stageWspr(
      callsign(fixture.service.snapshot().candidate, "IZ2ABC"), 0);
  fixture.runtime.failedAppliesRemaining = 2;
  WsprSaveSafetyContext context = fixture.cliContext();
  const WsprSaveOutcome result = fixture.service.saveWspr(1, context);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::PERSISTED_APPLY_FAILED),
      static_cast<uint8_t>(result.result));
  TEST_ASSERT_FALSE(result.runtimeApplySucceeded);
  TEST_ASSERT_FALSE(result.rollbackSucceeded);
  TEST_ASSERT_FALSE(fixture.service.runtimeSynchronized());
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", fixture.service.snapshot().saved.callsign);
  TEST_ASSERT_EQUAL_STRING("IW2NNV", fixture.service.snapshot().active.callsign);
}

void test_wspr_only_web_owner_is_required_and_operation_is_rf_safe() {
  constexpr char OWNER[] = "00112233445566778899aabbccddeeff";
  constexpr char OTHER[] = "ffeeddccbbaa99887766554433221100";
  SafeSaveFixture fixture;
  fixture.sessions.acquireWeb(OWNER, 1000, fixture.transmitter);
  fixture.service.stageWspr(
      callsign(fixture.service.snapshot().candidate, "IZ2ABC"), 0);
  WsprSaveSafetyContext wrong = {fixture.sessions, fixture.transmitter,
      ConfigurationSaveOwner::WEB, OTHER, 2000};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprSaveResult::CONFIG_MODE_REQUIRED),
      static_cast<uint8_t>(fixture.service.saveWspr(1, wrong).result));
  TEST_ASSERT_EQUAL_UINT16(0, fixture.storage.writes[0]);

  fixture.runtime.busyState = true;
  WsprSaveSafetyContext owned = {fixture.sessions, fixture.transmitter,
      ConfigurationSaveOwner::WEB, OWNER, 2000};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprSaveResult::RF_UNSAFE),
      static_cast<uint8_t>(fixture.service.saveWspr(1, owned).result));
  TEST_ASSERT_EQUAL_UINT16(0, fixture.storage.writes[0]);
}

void test_wspr_save_response_json_maps_results_and_partial_state() {
  char output[160];
  size_t length = 0;
  WsprSaveOutcome outcome;
  outcome.result = WsprSaveResult::SAVED_APPLIED;
  outcome.persistenceOccurred = true;
  outcome.runtimeApplySucceeded = true;
  TEST_ASSERT_TRUE(ConfigurationSaveJson::serialize(
      outcome, UINT32_MAX, output, sizeof(output), length));
  TEST_ASSERT_EQUAL_STRING(
      "{\"schema\":1,\"result\":\"saved_applied\","
      "\"revision\":4294967295,\"persisted\":true,\"applied\":true}\n",
      output);

  outcome.result = WsprSaveResult::PERSISTED_APPLY_FAILED;
  outcome.runtimeApplySucceeded = false;
  TEST_ASSERT_TRUE(ConfigurationSaveJson::serialize(
      outcome, 7, output, sizeof(output), length));
  TEST_ASSERT_NOT_NULL(strstr(output, "\"result\":\"persisted_apply_failed\""));
  TEST_ASSERT_NOT_NULL(strstr(output, "\"persisted\":true"));
  TEST_ASSERT_NOT_NULL(strstr(output, "\"applied\":false"));

  char tooSmall[8];
  TEST_ASSERT_FALSE(ConfigurationSaveJson::serialize(
      outcome, 7, tooSmall, sizeof(tooSmall), length));
  TEST_ASSERT_EQUAL_UINT32(0, length);
  TEST_ASSERT_EQUAL_STRING("", tooSmall);
}

void test_provisioning_wifi_save_is_isolated_verified_and_never_applied() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  FakeRuntime runtime;
  ConfigurationService service(manager, runtime);
  const BeaconSettings wsprBefore = manager.activeWspr();
  const WifiCredentials credentials = wifi("Field Network", "12345678");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ProvisioningWifiSaveResult::NOT_PROVISIONING),
      static_cast<uint8_t>(service.saveProvisioningWifi(credentials, false)));
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ProvisioningWifiSaveResult::SAVED),
      static_cast<uint8_t>(service.saveProvisioningWifi(credentials, true)));
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[1]);
  TEST_ASSERT_EQUAL_STRING("Field Network", manager.savedWifi().ssid);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
  TEST_ASSERT_FALSE(manager.candidateWifi().hasSsid());
  TEST_ASSERT_EQUAL_STRING(wsprBefore.callsign, manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_UINT8(0, runtime.applyCalls);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ProvisioningWifiSaveResult::UNCHANGED),
      static_cast<uint8_t>(service.saveProvisioningWifi(credentials, true)));
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[1]);
}


}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_owns_active_candidate_saved_and_revision);
  RUN_TEST(test_complete_candidate_validation_is_atomic);
  RUN_TEST(test_unconfigured_default_can_stage_and_save_valid_identity);
  RUN_TEST(test_cli_refresh_preserves_candidate_changes_from_another_interface);
  RUN_TEST(test_stale_revision_rejects_mutations);
  RUN_TEST(test_revision_wraparound_is_defined);
  RUN_TEST(test_discard_restores_saved_without_changing_active);
  RUN_TEST(test_web_discard_restores_only_wspr_and_preserves_wifi_candidate);
  RUN_TEST(test_cli_mode_selection_changes_candidate_and_active);
  RUN_TEST(test_future_web_mode_staging_is_candidate_only);
  RUN_TEST(test_save_verifies_persists_applies_and_does_not_write_wifi);
  RUN_TEST(test_partial_save_advances_saved_but_not_active);
  RUN_TEST(test_runtime_apply_failure_is_explicit_and_active_is_rolled_back);
  RUN_TEST(test_failed_runtime_rollback_is_reported_as_unsynchronized);
  RUN_TEST(test_busy_state_rejects_staging_mode_discard_and_save);
  RUN_TEST(test_wspr_only_save_requires_owner_and_checks_stale_revision_first);
  RUN_TEST(test_wspr_only_save_persists_applies_and_never_writes_wifi);
  RUN_TEST(test_wspr_only_unchanged_performs_no_write_apply_or_revision_change);
  RUN_TEST(test_wspr_only_reports_write_and_verification_failures_truthfully);
  RUN_TEST(test_wspr_only_apply_failure_preserves_saved_and_rolls_back_active);
  RUN_TEST(test_wspr_only_reconciles_saved_active_without_another_flash_write);
  RUN_TEST(test_wspr_only_reports_failed_runtime_rollback);
  RUN_TEST(test_wspr_only_web_owner_is_required_and_operation_is_rf_safe);
  RUN_TEST(test_wspr_save_response_json_maps_results_and_partial_state);
  RUN_TEST(test_provisioning_wifi_save_is_isolated_verified_and_never_applied);
  return UNITY_END();
}
