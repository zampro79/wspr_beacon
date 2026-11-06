#pragma once

#include <stdint.h>

#include "BeaconSettings.h"
#include "ConfigStorage.h"
#include "OperatingIdentity.h"
#include "WifiCredentials.h"

enum class ConfigOrigin : uint8_t { DEFAULTS, SAVED };
enum class ConfigSaveResult : uint8_t {
  NONE,
  DEFERRED,
  UNCHANGED,
  SAVED,
  PARTIAL,
  FAILED,
  INVALID
};

enum class WsprPersistenceResult : uint8_t {
  UNCHANGED,
  SAVED,
  INVALID,
  WRITE_FAILED,
  VERIFICATION_FAILED
};
enum class WifiPersistenceResult : uint8_t {
  UNCHANGED,
  SAVED,
  INVALID,
  WRITE_FAILED,
  VERIFICATION_FAILED
};

struct ConfigurationStatus {
  ConfigOrigin wsprOrigin = ConfigOrigin::DEFAULTS;
  ConfigOrigin wifiOrigin = ConfigOrigin::DEFAULTS;
  ConfigSaveResult lastSave = ConfigSaveResult::NONE;
  bool savePending = false;
  bool wsprDirty = false;
  bool wifiDirty = false;
  bool wsprRecordRejected = false;
  bool wifiRecordRejected = false;
  bool wsprMigrationPending = false;
  uint8_t wsprPayloadVersion = 0;
};

class ConfigurationManager {
 public:
  explicit ConfigurationManager(
      ConfigStorage &storage,
      const BeaconSettings &defaultWspr = BeaconSettings());

  void begin(bool factoryResetState = false,
             bool completedFactoryReset = false);
  void enterFactoryResetState(bool transactionCompleted = false);
  bool setWsprCandidate(const BeaconSettings &settings);
  bool applyWsprSelection(WsprBandId band, WsprFrequencyMode mode);
  bool setWifiCandidate(const WifiCredentials &credentials);
  void discardCandidates();
  void discardWsprCandidate();
  void loadDefaultsAsCandidates();
  bool requestSave();
  ConfigSaveResult processPending(bool txBusy);
  WsprPersistenceResult saveWsprOnly();
  WifiPersistenceResult saveWifiOnly(const WifiCredentials &credentials);
  bool restoreActiveWspr(const BeaconSettings &settings);

  const BeaconSettings &defaultWspr() const { return defaultWspr_; }
  const BeaconSettings &savedWspr() const { return savedWspr_; }
  const BeaconSettings &candidateWspr() const { return candidateWspr_; }
  const BeaconSettings &activeWspr() const { return activeWspr_; }
  OperatingIdentityState savedIdentityState() const {
    return operatingIdentityState(savedWspr_);
  }
  OperatingIdentityState candidateIdentityState() const {
    return operatingIdentityState(candidateWspr_);
  }
  OperatingIdentityState activeIdentityState() const {
    return operatingIdentityState(activeWspr_);
  }
  const WifiCredentials &savedWifi() const { return savedWifi_; }
  const WifiCredentials &candidateWifi() const { return candidateWifi_; }
  const WifiCredentials &activeWifi() const { return activeWifi_; }
  ConfigurationStatus status() const;

 private:
  ConfigStorage &storage_;
  BeaconSettings defaultWspr_;
  BeaconSettings savedWspr_;
  BeaconSettings candidateWspr_;
  BeaconSettings activeWspr_;
  WifiCredentials defaultWifi_;
  WifiCredentials savedWifi_;
  WifiCredentials candidateWifi_;
  WifiCredentials activeWifi_;
  ConfigOrigin wsprOrigin_ = ConfigOrigin::DEFAULTS;
  ConfigOrigin wifiOrigin_ = ConfigOrigin::DEFAULTS;
  ConfigSaveResult lastSave_ = ConfigSaveResult::NONE;
  uint32_t wsprSequence_ = 0;
  uint32_t wifiSequence_ = 0;
  uint8_t wsprSlot_ = 0;
  uint8_t wifiSlot_ = 0;
  uint8_t wsprPayloadVersion_ = 0;
  bool savePending_ = false;
  bool wsprRecordRejected_ = false;
  bool wifiRecordRejected_ = false;
  bool completedFactoryReset_ = false;

  WsprPersistenceResult saveWspr();
  WifiPersistenceResult saveWifi();
  bool wsprDirty() const;
  bool wsprMigrationPending() const;
  bool wifiDirty() const;
};
