#include <unity.h>

#include <string.h>

#include "ConfigurationManager.h"
#include "ApSetupBootDecision.h"
#include "ApSetupMarker.h"
#include "BootModeController.h"
#include "FactoryResetCore.h"
#include "FactoryResetService.h"
#include "FactoryResetJson.h"
#include "ManualTxController.h"
#include "PersistentConfigRecord.h"

namespace {
class FakeStorage : public ConfigStorage {
 public:
  struct Blob {
    uint8_t data[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
    size_t length = 0;
    bool present = false;
  };

  bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
            size_t capacity, size_t &length) override {
    length = 0;
    if (failRecordReads) return false;
    const Blob &source = records[type == ConfigRecordType::WSPR ? 0 : 1][slot];
    if (!source.present || source.length > capacity) return false;
    memcpy(data, source.data, source.length);
    length = source.length;
    return true;
  }
  bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) override {
    Blob &target = records[type == ConfigRecordType::WSPR ? 0 : 1][slot];
    if (type == ConfigRecordType::WIFI && failWifiWrite) {
      target.present = true;
      target.length = length / 2U;
      memcpy(target.data, data, target.length);
      return false;
    }
    target.present = true;
    target.length = length;
    memcpy(target.data, data, length);
    return true;
  }
  StorageReadResult readFactoryMarker(uint8_t slot, uint8_t *data,
                                      size_t capacity,
                                      size_t &length) override {
    length = 0;
    if (failMarkerReads) return StorageReadResult::ERROR;
    const Blob &source = markers[slot];
    if (!source.present) return StorageReadResult::ABSENT;
    if (source.length > capacity) return StorageReadResult::ERROR;
    memcpy(data, source.data, source.length);
    length = source.length;
    return StorageReadResult::OK;
  }
  bool writeFactoryMarker(uint8_t slot, const uint8_t *data,
                          size_t length) override {
    ++operations;
    if (failOperation == operations) {
      if (partialFailure) {
        markers[slot].present = true;
        markers[slot].length = length / 2U;
        memcpy(markers[slot].data, data, length / 2U);
      }
      return false;
    }
    Blob &target = markers[slot];
    target.present = true;
    target.length = length;
    memcpy(target.data, data, length);
    if (corruptMarkerWriteOperation == operations) target.data[0] ^= 0xFFU;
    return true;
  }
  bool invalidate(ConfigRecordType type, uint8_t slot) override {
    ++operations;
    if (failOperation == operations) return false;
    Blob &target = records[type == ConfigRecordType::WSPR ? 0 : 1][slot];
    target.present = true;
    target.length = type == ConfigRecordType::WSPR
                        ? PersistentConfigRecord::WSPR_RECORD_SIZE
                        : PersistentConfigRecord::WIFI_RECORD_SIZE;
    memset(target.data, corruptInvalidationOperation == operations ? 0xA5 : 0,
           target.length);
    return true;
  }
  bool clearFactoryMarkers() override {
    if (failMarkerClear) return false;
    markers[0] = Blob();
    markers[1] = Blob();
    return true;
  }
  StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                      size_t &length) override {
    length = 0;
    if (failApMarkerReads) return StorageReadResult::ERROR;
    if (!apMarker.present) return StorageReadResult::ABSENT;
    if (apMarker.length > capacity) return StorageReadResult::ERROR;
    memcpy(data, apMarker.data, apMarker.length);
    length = apMarker.length;
    return StorageReadResult::OK;
  }
  bool writeApSetupMarker(const uint8_t *data, size_t length) override {
    ++apMarkerWrites;
    if (failApMarkerWrite) return false;
    apMarker.present = true;
    apMarker.length = partialApMarkerWrite ? length / 2U : length;
    memcpy(apMarker.data, data, apMarker.length);
    if (corruptApMarkerWrite) apMarker.data[0] ^= 1U;
    return true;
  }

  Blob records[2][2];
  Blob markers[2];
  Blob apMarker;
  uint8_t apMarkerWrites = 0;
  bool failApMarkerReads = false;
  bool failApMarkerWrite = false;
  bool partialApMarkerWrite = false;
  bool corruptApMarkerWrite = false;
  uint8_t operations = 0;
  uint8_t failOperation = 0;
  uint8_t corruptInvalidationOperation = 0;
  uint8_t corruptMarkerWriteOperation = 0;
  bool partialFailure = false;
  bool failMarkerReads = false;
  bool failRecordReads = false;
  bool failMarkerClear = false;
  bool failWifiWrite = false;
};

class FakeRfDriver : public RfDriver {
 public:
  bool outputEnabled = false;
  uint32_t enableCalls = 0;
  uint32_t disableCalls = 0;
  bool detected() override { return true; }
  bool initialize(uint32_t, int32_t) override { return true; }
  bool setFrequency(uint64_t) override { return true; }
  void enableOutput() override { outputEnabled = true; ++enableCalls; }
  void disableOutput() override { outputEnabled = false; ++disableCalls; }
};

BeaconConfig configuredConfig() {
  BeaconConfig config;
  strcpy(config.callsign, "IW2NNV");
  return config;
}

struct Runtime {
  BeaconConfig config;
  FakeRfDriver driver;
  RfSafetyController rf;
  JTEncode encoder;
  WsprTransmitter transmitter;
  ManualTxController manual;
  Runtime()
      : config(configuredConfig()),
        rf(driver, config.si5351ReferenceHz, config.si5351Correction),
        transmitter(config, rf, encoder), manual(transmitter) {
    TEST_ASSERT_TRUE(transmitter.begin());
  }
};

WifiCredentials wifi(const char *ssid, const char *password) {
  WifiCredentials result;
  TEST_ASSERT_TRUE(result.setSsid(ssid));
  TEST_ASSERT_TRUE(result.setPassword(password));
  return result;
}

void seedSavedConfiguration(FakeStorage &storage) {
  ConfigurationManager manager(storage);
  manager.begin();
  BeaconSettings wspr;
  strcpy(wspr.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wspr));
  TEST_ASSERT_TRUE(manager.setWifiCandidate(wifi("Saved network", "saved secret")));
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
                          static_cast<uint8_t>(manager.processPending(false)));
}

void assertFactoryState(const ConfigurationManager &manager) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(manager.activeIdentityState()));
  TEST_ASSERT_EQUAL_STRING("", manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("", manager.candidateWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("", manager.savedWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("JN45", manager.activeWspr().fixedLocator);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
                          static_cast<uint8_t>(manager.activeWspr().band));
  TEST_ASSERT_EQUAL_UINT32(BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ,
                           manager.activeWspr().baseFrequencyHz);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
  TEST_ASSERT_FALSE(manager.savedWifi().hasSsid());
}

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
  result.source = LocatorSource::FIXED;
  return result;
}
}  // namespace

void test_fresh_storage_is_normal_and_preserves_profile_defaults() {
  FakeStorage storage;
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
                          static_cast<uint8_t>(reset.recover(runtime.manual)));
  TEST_ASSERT_TRUE(runtime.manual.permitted());
  ConfigurationManager manager(storage);
  manager.begin(reset.factoryStateRequired());
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
  TEST_ASSERT_EQUAL_UINT8(0, storage.operations);
}

void test_valid_saved_records_load_without_automatic_reset() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
                          static_cast<uint8_t>(reset.recover(runtime.manual)));
  ConfigurationManager manager(storage);
  manager.begin(reset.factoryStateRequired());
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("Saved network", manager.activeWifi().ssid);
}

void test_completed_reset_is_unconfigured_and_survives_reboot() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_FALSE(runtime.manual.permitted());
  TEST_ASSERT_FALSE(runtime.driver.outputEnabled);
  assertFactoryState(manager);

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
  ConfigurationManager rebooted(storage);
  rebooted.begin(rebootReset.factoryStateRequired(), true);
  assertFactoryState(rebooted);
}

void test_power_loss_before_marker_preserves_records_and_normal_boot() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  storage.operations = 0;
  storage.failOperation = 1;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FactoryResetResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  storage.failOperation = 0;
  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(false);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", rebooted.activeWspr().callsign);
}

void test_recovery_completes_every_interrupted_transaction_step() {
  for (uint8_t interruptedOperation = 2; interruptedOperation <= 6;
       ++interruptedOperation) {
    FakeStorage storage;
    seedSavedConfiguration(storage);
    storage.operations = 0;
    storage.failOperation = interruptedOperation;
    ConfigurationManager manager(storage);
    manager.begin();
    Runtime runtime;
    FactoryResetCore reset(storage);
    TEST_ASSERT_NOT_EQUAL(
        static_cast<uint8_t>(FactoryResetResult::COMPLETED),
        static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                         runtime.transmitter)));
    storage.failOperation = 0;
    Runtime rebootRuntime;
    FactoryResetCore rebootReset(storage);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::RECOVERED),
        static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
    TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
    ConfigurationManager rebooted(storage);
    rebooted.begin(rebootReset.factoryStateRequired(), true);
    assertFactoryState(rebooted);
  }
}

void test_torn_completion_marker_recovers_from_valid_pending_marker() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  storage.operations = 0;
  storage.failOperation = 6;
  storage.partialFailure = true;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FactoryResetResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  storage.failOperation = 0;
  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::RECOVERED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
}

void test_corrupt_or_unreadable_marker_fails_closed_without_record_use() {
  for (uint8_t mode = 0; mode < 2; ++mode) {
    FakeStorage storage;
    seedSavedConfiguration(storage);
    if (mode == 0) {
      storage.markers[0].present = true;
      storage.markers[0].length = FactoryResetCore::MARKER_SIZE;
      memset(storage.markers[0].data, 0xA5, FactoryResetCore::MARKER_SIZE);
    } else {
      storage.failMarkerReads = true;
    }
    Runtime runtime;
    FactoryResetCore reset(storage);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(FactoryResetBootState::FAIL_CLOSED),
        static_cast<uint8_t>(reset.recover(runtime.manual)));
    TEST_ASSERT_FALSE(runtime.manual.permitted());
    ConfigurationManager manager(storage);
    manager.begin(reset.factoryStateRequired());
    assertFactoryState(manager);
  }
}

void test_invalidation_readback_failure_remains_pending_and_fail_closed() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  storage.operations = 0;
  storage.corruptInvalidationOperation = 3;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FactoryResetResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_FALSE(runtime.manual.permitted());
  TEST_ASSERT_TRUE(reset.factoryStateRequired());
}

void test_marker_readback_failure_stops_before_slot_invalidation() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  storage.operations = 0;
  storage.corruptMarkerWriteOperation = 1;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FactoryResetResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(1, storage.operations);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  TEST_ASSERT_FALSE(runtime.manual.permitted());
}

void test_repeated_reset_is_idempotent_without_additional_writes() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  const uint8_t operations = storage.operations;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FactoryResetResult::ALREADY_COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(operations, storage.operations);
}

void test_explicit_valid_identity_save_exits_completed_reset_for_safe_boot() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  BeaconSettings configured = manager.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(configured));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprPersistenceResult::SAVED),
                          static_cast<uint8_t>(manager.saveWsprOnly()));

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(false);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", rebooted.activeWspr().callsign);
  TEST_ASSERT_FALSE(rebooted.activeWifi().hasSsid());
}

void test_failed_marker_clear_keeps_completed_reset_fail_closed() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  BeaconSettings configured = manager.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(configured));
  storage.failMarkerClear = true;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprPersistenceResult::VERIFICATION_FAILED),
      static_cast<uint8_t>(manager.saveWsprOnly()));
  storage.failMarkerClear = false;
  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
}

void test_post_reset_wifi_is_loaded_after_reboot_while_wspr_stays_unconfigured() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));

  const WifiCredentials replacement = wifi("Recovery network", "new secret");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(replacement)));
  TEST_ASSERT_TRUE(storage.markers[0].present || storage.markers[1].present);

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(true, true);
  TEST_ASSERT_EQUAL_STRING("Recovery network", rebooted.savedWifi().ssid);
  TEST_ASSERT_EQUAL_STRING("Recovery network", rebooted.candidateWifi().ssid);
  TEST_ASSERT_EQUAL_STRING("Recovery network", rebooted.activeWifi().ssid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
      static_cast<uint8_t>(rebooted.status().wifiOrigin));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED),
      static_cast<uint8_t>(rebooted.activeIdentityState()));

  BootModeController boot;
  const bool usableWifi = rebooted.status().wifiOrigin == ConfigOrigin::SAVED &&
                          rebooted.activeWifi().complete();
  boot.begin(usableWifi, 0);
  TEST_ASSERT_FALSE(boot.enterApRequested());
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());

  TEST_ASSERT_TRUE(rebootRuntime.transmitter.setOperatingIdentityUnconfigured(
      rebooted.activeWspr()));
  TEST_ASSERT_TRUE(rebootRuntime.manual.turnOn(true));
  TEST_ASSERT_TRUE(rebootRuntime.rf.recover());
  rebootRuntime.transmitter.setTxAdmissionAllowed(true);
  rebootRuntime.transmitter.update(authorizedTime(235000UL), location(),
                                   5000UL, 1000UL);
  TEST_ASSERT_EQUAL_UINT32(0, rebootRuntime.encoder.encodeCalls);
  TEST_ASSERT_FALSE(rebootRuntime.driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(rebootRuntime.transmitter.state()));

  Runtime secondRuntime;
  FactoryResetCore secondReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(secondReset.recover(secondRuntime.manual)));
  ConfigurationManager secondBoot(storage);
  secondBoot.begin(true, true);
  TEST_ASSERT_EQUAL_STRING("Recovery network", secondBoot.activeWifi().ssid);
  TEST_ASSERT_EQUAL_STRING("", secondBoot.activeWspr().callsign);
}

void test_interrupted_post_reset_wifi_save_remains_fail_closed() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  storage.failWifiWrite = true;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::WRITE_FAILED),
      static_cast<uint8_t>(manager.saveWifiOnly(
          wifi("Interrupted network", "new secret"))));
  storage.failWifiWrite = false;

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(true, true);
  TEST_ASSERT_FALSE(rebooted.activeWifi().hasSsid());
  TEST_ASSERT_TRUE(rebooted.status().wifiRecordRejected);
  BootModeController boot;
  boot.begin(false, 0);
  TEST_ASSERT_TRUE(boot.enterApRequested());
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
}

void test_post_reset_wifi_ab_recovers_previous_verified_record() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(wifi("First network", "first pass"))));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(wifi("Second network", "second pass"))));

  storage.records[1][0].data[14] ^= 0x80U;
  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::COMPLETED),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(true, true);
  TEST_ASSERT_EQUAL_STRING("First network", rebooted.activeWifi().ssid);
  TEST_ASSERT_TRUE(rebooted.status().wifiRecordRejected);
  TEST_ASSERT_EQUAL_STRING("", rebooted.activeWspr().callsign);
}

void test_wifi_first_then_wspr_save_preserves_both_domains() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(wifi("WiFi first", "first pass"))));
  BeaconSettings configured = manager.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(configured));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWsprOnly()));
  TEST_ASSERT_FALSE(storage.markers[0].present);
  TEST_ASSERT_FALSE(storage.markers[1].present);

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(false);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", rebooted.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("WiFi first", rebooted.activeWifi().ssid);
}

void test_wspr_first_then_wifi_save_preserves_both_domains() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  BeaconSettings configured = manager.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(configured));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWsprOnly()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(wifi("WiFi second", "second pass"))));

  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetBootState::NORMAL),
      static_cast<uint8_t>(rebootReset.recover(rebootRuntime.manual)));
  ConfigurationManager rebooted(storage);
  rebooted.begin(false);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", rebooted.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("WiFi second", rebooted.activeWifi().ssid);
}

void test_reset_aborts_active_tx_and_prevents_new_or_catchup_frame() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  runtime.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_TRUE(runtime.transmitter.releasePreparation());
  runtime.transmitter.update(authorizedTime(240000UL), location(), 10000UL,
                             5001000UL);
  TEST_ASSERT_TRUE(runtime.driver.outputEnabled);
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_FALSE(runtime.driver.outputEnabled);
  TEST_ASSERT_FALSE(runtime.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::RF_INHIBITED),
                          static_cast<uint8_t>(runtime.transmitter.state()));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxFrameResult::INTERRUPTED_MANUAL_TX_OFF),
      static_cast<uint8_t>(runtime.transmitter.telemetry().lastFrame.result));
  const uint32_t encoded = runtime.encoder.encodeCalls;
  runtime.transmitter.update(authorizedTime(240001UL), location(), 10001UL,
                             5002000UL);
  TEST_ASSERT_EQUAL_UINT32(encoded, runtime.encoder.encodeCalls);
  TEST_ASSERT_FALSE(runtime.driver.outputEnabled);
}

void test_reset_cancels_preparing_before_persistence_changes() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  runtime.transmitter.update(authorizedTime(235000UL), location(), 5000UL,
                             1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TransmitterState::PREPARING),
                          static_cast<uint8_t>(runtime.transmitter.state()));
  TEST_ASSERT_TRUE(runtime.transmitter.preparedSnapshot().valid);
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetResult::COMPLETED),
      static_cast<uint8_t>(reset.reset(runtime.manual, manager,
                                       runtime.transmitter)));
  TEST_ASSERT_FALSE(runtime.transmitter.preparedSnapshot().valid);
  TEST_ASSERT_FALSE(runtime.driver.outputEnabled);
  TEST_ASSERT_FALSE(runtime.manual.permitted());
}

void test_factory_reset_payload_is_exact_bounded_and_revisioned() {
  FactoryResetRequest request;
  const char valid[] =
      "{\"expected_revision\":42,\"confirm\":\"FACTORY RESET\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetParseResult::OK),
      static_cast<uint8_t>(FactoryResetJson::parse(valid, strlen(valid), request)));
  TEST_ASSERT_EQUAL_UINT32(42, request.expectedRevision);
  const char duplicate[] =
      "{\"expected_revision\":1,\"confirm\":\"FACTORY RESET\",\"confirm\":\"FACTORY RESET\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetParseResult::DUPLICATE_FIELD),
      static_cast<uint8_t>(FactoryResetJson::parse(duplicate, strlen(duplicate), request)));
  const char unknown[] =
      "{\"expected_revision\":1,\"confirm\":\"FACTORY RESET\",\"retry\":true}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetParseResult::UNKNOWN_FIELD),
      static_cast<uint8_t>(FactoryResetJson::parse(unknown, strlen(unknown), request)));
  const char wrong[] =
      "{\"expected_revision\":1,\"confirm\":\"factory reset\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(FactoryResetJson::parse(wrong, strlen(wrong), request)));
}

void test_shared_service_requires_exclusive_owner_and_invalidates_sessions() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore core(storage);
  ConfigurationSessionController sessions;
  FactoryResetService service(core, manager, sessions, runtime.manual,
                              runtime.transmitter);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetServiceResult::SESSION_REQUIRED),
      static_cast<uint8_t>(service.execute(FactoryResetOwner::CLI, nullptr,
                                           sessions.revision(), 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionResult::OK),
      static_cast<uint8_t>(sessions.acquireCli(runtime.transmitter)));
  const uint32_t revision = sessions.revision();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetServiceResult::STALE_REVISION),
      static_cast<uint8_t>(service.execute(FactoryResetOwner::CLI, nullptr,
                                           revision + 1U, 0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FactoryResetServiceResult::COMPLETED),
      static_cast<uint8_t>(service.execute(FactoryResetOwner::CLI, nullptr,
                                           revision, 0)));
  TEST_ASSERT_TRUE(service.resetCompletedThisBoot());
  TEST_ASSERT_FALSE(sessions.cliActive());
  TEST_ASSERT_FALSE(sessions.webActive());
  TEST_ASSERT_TRUE(sessions.resumeGuardActive());
  TEST_ASSERT_FALSE(runtime.manual.permitted());
  assertFactoryState(manager);
}

void test_ap_setup_pending_is_cancelled_before_factory_completion() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::COMPLETED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  TEST_ASSERT_FALSE(runtime.manual.permitted());
  Runtime rebootRuntime;
  FactoryResetCore rebootReset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::COMPLETED,
                        rebootReset.recover(rebootRuntime.manual));
  ConfigurationManager rebooted(storage);
  rebooted.begin(true, true);
  BeaconSettings configured = rebooted.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(rebooted.setWsprCandidate(configured));
  TEST_ASSERT_EQUAL_INT(WsprPersistenceResult::SAVED, rebooted.saveWsprOnly());
  TEST_ASSERT_FALSE(storage.markers[0].present);
  TEST_ASSERT_FALSE(storage.markers[1].present);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  Runtime nextRuntime;
  FactoryResetCore nextReset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::NORMAL,
                        nextReset.recover(nextRuntime.manual));
  TEST_ASSERT_TRUE(ApSetupBootDecision::trySta(
      true, true, nextReset.bootState(), ap.read()));
}

void test_ap_cancellation_failure_keeps_factory_pending_and_rf_closed() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  storage.failApMarkerWrite = true;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::VERIFICATION_FAILED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  TEST_ASSERT_FALSE(runtime.manual.permitted());
  TEST_ASSERT_FALSE(runtime.driver.outputEnabled);
  TEST_ASSERT_TRUE(storage.markers[0].present);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, ap.read());
  Runtime failedReboot;
  FactoryResetCore failedRecovery(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::FAIL_CLOSED,
                        failedRecovery.recover(failedReboot.manual));
  TEST_ASSERT_FALSE(failedReboot.manual.permitted());
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(
      true, true, failedRecovery.bootState(), ap.read()));
  storage.failApMarkerWrite = false;
  Runtime recoveredRuntime;
  FactoryResetCore recovered(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::RECOVERED,
                        recovered.recover(recoveredRuntime.manual));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
}

void test_power_loss_after_ap_cancellation_recovers_factory_reset() {
  FakeStorage storage;
  seedSavedConfiguration(storage);
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  storage.operations = 0;
  storage.failOperation = 2;  // First WSPR slot invalidation, after AP cancellation.
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::VERIFICATION_FAILED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  TEST_ASSERT_TRUE(storage.markers[0].present);
  storage.failOperation = 0;
  Runtime rebootRuntime;
  FactoryResetCore recovered(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::RECOVERED,
                        recovered.recover(rebootRuntime.manual));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
}

void test_ap_cancellation_readback_failure_keeps_factory_marker() {
  FakeStorage storage;
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  storage.corruptApMarkerWrite = true;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::VERIFICATION_FAILED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  TEST_ASSERT_TRUE(storage.markers[0].present);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, ap.read());
  storage.corruptApMarkerWrite = false;
  Runtime rebootRuntime;
  FactoryResetCore recovered(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::RECOVERED,
                        recovered.recover(rebootRuntime.manual));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
}

void test_wspr_save_does_not_clear_factory_marker_on_ap_uncertainty() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::COMPLETED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  BeaconSettings configured = manager.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(manager.setWsprCandidate(configured));
  storage.failApMarkerReads = true;
  TEST_ASSERT_EQUAL_INT(WsprPersistenceResult::VERIFICATION_FAILED,
                        manager.saveWsprOnly());
  TEST_ASSERT_TRUE(storage.markers[0].present || storage.markers[1].present);
  storage.failApMarkerReads = false;
  Runtime rebootRuntime;
  FactoryResetCore recovered(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::COMPLETED,
                        recovered.recover(rebootRuntime.manual));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  ConfigurationManager rebooted(storage);
  rebooted.begin(true, true);
  configured = rebooted.candidateWspr();
  strcpy(configured.callsign, "IZ2ABC");
  TEST_ASSERT_TRUE(rebooted.setWsprCandidate(configured));
  TEST_ASSERT_EQUAL_INT(WsprPersistenceResult::SAVED, rebooted.saveWsprOnly());
  TEST_ASSERT_FALSE(storage.markers[0].present);
  TEST_ASSERT_FALSE(storage.markers[1].present);
}

void test_corrupt_ap_marker_is_cancelled_under_durable_factory_marker() {
  FakeStorage storage;
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  storage.apMarker.data[0] ^= 1U;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::COMPLETED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  TEST_ASSERT_FALSE(runtime.manual.permitted());
}

void test_completed_factory_marker_cancels_later_ap_request() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  Runtime runtime;
  FactoryResetCore reset(storage);
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::COMPLETED,
                        reset.reset(runtime.manual, manager, runtime.transmitter));
  ApSetupMarker ap(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, ap.request());
  Runtime rebootRuntime;
  FactoryResetCore recovered(storage);
  storage.failApMarkerWrite = true;
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::FAIL_CLOSED,
                        recovered.recover(rebootRuntime.manual));
  TEST_ASSERT_FALSE(rebootRuntime.manual.permitted());
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(
      true, true, recovered.bootState(), ap.read()));
  storage.failApMarkerWrite = false;
  TEST_ASSERT_EQUAL_INT(FactoryResetBootState::COMPLETED,
                        recovered.recover(rebootRuntime.manual));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, ap.read());
  TEST_ASSERT_EQUAL_INT(FactoryResetResult::ALREADY_COMPLETED,
      recovered.reset(rebootRuntime.manual, manager, rebootRuntime.transmitter));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_storage_is_normal_and_preserves_profile_defaults);
  RUN_TEST(test_valid_saved_records_load_without_automatic_reset);
  RUN_TEST(test_completed_reset_is_unconfigured_and_survives_reboot);
  RUN_TEST(test_power_loss_before_marker_preserves_records_and_normal_boot);
  RUN_TEST(test_recovery_completes_every_interrupted_transaction_step);
  RUN_TEST(test_torn_completion_marker_recovers_from_valid_pending_marker);
  RUN_TEST(test_corrupt_or_unreadable_marker_fails_closed_without_record_use);
  RUN_TEST(test_invalidation_readback_failure_remains_pending_and_fail_closed);
  RUN_TEST(test_marker_readback_failure_stops_before_slot_invalidation);
  RUN_TEST(test_repeated_reset_is_idempotent_without_additional_writes);
  RUN_TEST(test_explicit_valid_identity_save_exits_completed_reset_for_safe_boot);
  RUN_TEST(test_failed_marker_clear_keeps_completed_reset_fail_closed);
  RUN_TEST(test_post_reset_wifi_is_loaded_after_reboot_while_wspr_stays_unconfigured);
  RUN_TEST(test_interrupted_post_reset_wifi_save_remains_fail_closed);
  RUN_TEST(test_post_reset_wifi_ab_recovers_previous_verified_record);
  RUN_TEST(test_wifi_first_then_wspr_save_preserves_both_domains);
  RUN_TEST(test_wspr_first_then_wifi_save_preserves_both_domains);
  RUN_TEST(test_reset_aborts_active_tx_and_prevents_new_or_catchup_frame);
  RUN_TEST(test_reset_cancels_preparing_before_persistence_changes);
  RUN_TEST(test_factory_reset_payload_is_exact_bounded_and_revisioned);
  RUN_TEST(test_shared_service_requires_exclusive_owner_and_invalidates_sessions);
  RUN_TEST(test_ap_setup_pending_is_cancelled_before_factory_completion);
  RUN_TEST(test_ap_cancellation_failure_keeps_factory_pending_and_rf_closed);
  RUN_TEST(test_power_loss_after_ap_cancellation_recovers_factory_reset);
  RUN_TEST(test_ap_cancellation_readback_failure_keeps_factory_marker);
  RUN_TEST(test_wspr_save_does_not_clear_factory_marker_on_ap_uncertainty);
  RUN_TEST(test_corrupt_ap_marker_is_cancelled_under_durable_factory_marker);
  RUN_TEST(test_completed_factory_marker_cancels_later_ap_request);
  return UNITY_END();
}
