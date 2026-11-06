#pragma once

#include <stdint.h>

#include "ConfigurationManager.h"
#include "ConfigurationSessionController.h"

class ConfigurationRuntime {
 public:
  virtual ~ConfigurationRuntime() = default;
  virtual bool busy() const = 0;
  virtual bool applyWspr(const BeaconSettings &settings) = 0;
  virtual void applyPowerSave(bool enabled) { (void)enabled; }
  virtual bool selectFrequencyMode(WsprFrequencyMode mode) = 0;
};

enum class ConfigurationOperationResult : uint8_t {
  OK,
  UNCHANGED,
  INVALID,
  STALE_REVISION,
  RF_BUSY,
  SAVE_PARTIAL,
  SAVE_FAILED,
  RUNTIME_APPLY_FAILED
};

struct ConfigurationSnapshot {
  uint32_t revision = 0;
  BeaconSettings active;
  BeaconSettings candidate;
  BeaconSettings saved;
  ConfigurationStatus status;
  bool activeDiffersSaved = false;
  bool runtimeSynchronized = true;
};

struct ConfigurationSaveOutcome {
  ConfigurationOperationResult result = ConfigurationOperationResult::SAVE_FAILED;
  ConfigSaveResult persistence = ConfigSaveResult::NONE;
  bool wsprChanged = false;
  bool wifiChanged = false;
};

enum class ConfigurationSaveOwner : uint8_t { CLI, WEB };

struct WsprSaveSafetyContext {
  WsprSaveSafetyContext(ConfigurationSessionController &sessionController,
                        WsprTransmitter &wsprTransmitter,
                        ConfigurationSaveOwner saveOwner,
                        const char *sessionId, uint32_t observationMillis)
      : sessions(sessionController), transmitter(wsprTransmitter),
        owner(saveOwner), webSessionId(sessionId),
        nowMillis(observationMillis) {}

  ConfigurationSessionController &sessions;
  WsprTransmitter &transmitter;
  ConfigurationSaveOwner owner;
  const char *webSessionId;
  uint32_t nowMillis;
};

enum class WsprSaveResult : uint8_t {
  SAVED_APPLIED,
  UNCHANGED,
  STALE_REVISION,
  CONFIG_MODE_REQUIRED,
  RF_UNSAFE,
  INVALID_CONFIGURATION,
  PERSISTENCE_FAILED,
  VERIFICATION_FAILED,
  PERSISTED_APPLY_FAILED
};

struct WsprSaveOutcome {
  WsprSaveResult result = WsprSaveResult::PERSISTENCE_FAILED;
  WsprPersistenceResult persistence = WsprPersistenceResult::UNCHANGED;
  bool persistenceOccurred = false;
  bool runtimeApplyAttempted = false;
  bool runtimeApplySucceeded = false;
  bool rollbackSucceeded = false;
};

enum class ProvisioningWifiSaveResult : uint8_t {
  SAVED,
  UNCHANGED,
  INVALID,
  NOT_PROVISIONING,
  PERSISTENCE_FAILED,
  VERIFICATION_FAILED
};

class ConfigurationService {
 public:
  ConfigurationService(ConfigurationManager &manager,
                       ConfigurationRuntime &runtime)
      : manager_(manager), runtime_(runtime) {}

  ConfigurationSnapshot snapshot() const;
  uint32_t revision() const { return revision_; }
  bool runtimeSynchronized() const { return runtimeSynchronized_; }

  ConfigurationOperationResult stageWspr(const BeaconSettings &settings,
                                         uint32_t expectedRevision);
  ConfigurationOperationResult stageWifi(const WifiCredentials &credentials,
                                         uint32_t expectedRevision);
  ConfigurationOperationResult selectFrequencyMode(
      WsprFrequencyMode mode, uint32_t expectedRevision);
  ConfigurationOperationResult discard(uint32_t expectedRevision);
  ConfigurationOperationResult discardWspr(uint32_t expectedRevision);
  ConfigurationOperationResult defaults(uint32_t expectedRevision);
  ConfigurationSaveOutcome save(uint32_t expectedRevision);
  WsprSaveOutcome saveWspr(uint32_t expectedRevision,
                           WsprSaveSafetyContext &safety);
  ProvisioningWifiSaveResult saveProvisioningWifi(
      const WifiCredentials &credentials, bool provisioningActive);

  const ConfigurationManager &manager() const { return manager_; }
  ConfigurationManager &manager() { return manager_; }

  static uint32_t nextRevision(uint32_t revision) { return revision + 1U; }

 private:
  ConfigurationManager &manager_;
  ConfigurationRuntime &runtime_;
  uint32_t revision_ = 0;
  bool runtimeSynchronized_ = true;

  bool stale(uint32_t expectedRevision) const {
    return expectedRevision != revision_;
  }
  void changed() { revision_ = nextRevision(revision_); }
};
