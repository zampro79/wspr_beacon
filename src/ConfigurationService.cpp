#include "ConfigurationService.h"

#include <string.h>

#include "ConfigValidator.h"

namespace {
bool sameWspr(const BeaconSettings &left, const BeaconSettings &right) {
  return strcmp(left.callsign, right.callsign) == 0 &&
         strcmp(left.fixedLocator, right.fixedLocator) == 0 &&
         left.baseFrequencyHz == right.baseFrequencyHz &&
         left.frequencyOffsetHz == right.frequencyOffsetHz &&
         left.si5351Correction == right.si5351Correction &&
         left.powerDbm == right.powerDbm &&
         left.transmitIntervalMinutes == right.transmitIntervalMinutes &&
         left.band == right.band && left.frequencyMode == right.frequencyMode &&
         left.powerSave == right.powerSave;
}

bool sameWifi(const WifiCredentials &left, const WifiCredentials &right) {
  return strcmp(left.ssid, right.ssid) == 0 &&
         strcmp(left.password, right.password) == 0 && left.open == right.open;
}
}  // namespace

ConfigurationSnapshot ConfigurationService::snapshot() const {
  ConfigurationSnapshot result;
  result.revision = revision_;
  result.active = manager_.activeWspr();
  result.candidate = manager_.candidateWspr();
  result.saved = manager_.savedWspr();
  result.status = manager_.status();
  result.activeDiffersSaved = !sameWspr(result.active, result.saved);
  result.runtimeSynchronized = runtimeSynchronized_;
  return result;
}

ConfigurationOperationResult ConfigurationService::stageWspr(
    const BeaconSettings &settings, uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;
  if (ConfigValidator::validate(settings) != ConfigValidation::VALID) {
    return ConfigurationOperationResult::INVALID;
  }
  if (sameWspr(settings, manager_.candidateWspr())) {
    return ConfigurationOperationResult::UNCHANGED;
  }
  if (!manager_.setWsprCandidate(settings)) {
    return ConfigurationOperationResult::INVALID;
  }
  changed();
  return ConfigurationOperationResult::OK;
}

ConfigurationOperationResult ConfigurationService::stageWifi(
    const WifiCredentials &credentials, uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;
  if (sameWifi(credentials, manager_.candidateWifi())) {
    return ConfigurationOperationResult::UNCHANGED;
  }
  if (!manager_.setWifiCandidate(credentials)) {
    return ConfigurationOperationResult::INVALID;
  }
  changed();
  return ConfigurationOperationResult::OK;
}

ProvisioningWifiSaveResult ConfigurationService::saveProvisioningWifi(
    const WifiCredentials &credentials, bool provisioningActive) {
  if (!provisioningActive) return ProvisioningWifiSaveResult::NOT_PROVISIONING;
  if (!credentials.complete() ||
      !WifiCredentials::validSsid(credentials.ssid) ||
      (credentials.open ? credentials.hasPassword()
                        : !WifiCredentials::validPassword(credentials.password))) {
    return ProvisioningWifiSaveResult::INVALID;
  }
  const WifiPersistenceResult result = manager_.saveWifiOnly(credentials);
  switch (result) {
    case WifiPersistenceResult::SAVED:
      changed();
      return ProvisioningWifiSaveResult::SAVED;
    case WifiPersistenceResult::UNCHANGED:
      return ProvisioningWifiSaveResult::UNCHANGED;
    case WifiPersistenceResult::INVALID:
      return ProvisioningWifiSaveResult::INVALID;
    case WifiPersistenceResult::WRITE_FAILED:
      return ProvisioningWifiSaveResult::PERSISTENCE_FAILED;
    case WifiPersistenceResult::VERIFICATION_FAILED:
      return ProvisioningWifiSaveResult::VERIFICATION_FAILED;
  }
  return ProvisioningWifiSaveResult::PERSISTENCE_FAILED;
}

ConfigurationOperationResult ConfigurationService::selectFrequencyMode(
    WsprFrequencyMode mode, uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;

  BeaconSettings candidate = manager_.candidateWspr();
  candidate.frequencyMode = mode;
  if (ConfigValidator::validate(candidate) != ConfigValidation::VALID) {
    return ConfigurationOperationResult::INVALID;
  }
  const bool candidateChanged =
      !sameWspr(candidate, manager_.candidateWspr());
  const bool activeChanged = manager_.activeWspr().frequencyMode != mode;
  if (!candidateChanged && !activeChanged) {
    return ConfigurationOperationResult::UNCHANGED;
  }

  const WsprFrequencyMode previousMode = manager_.activeWspr().frequencyMode;
  if (!runtime_.selectFrequencyMode(mode)) {
    return ConfigurationOperationResult::RF_BUSY;
  }
  if (!manager_.setWsprCandidate(candidate) ||
      !manager_.applyWsprSelection(candidate.band, mode)) {
    runtime_.selectFrequencyMode(previousMode);
    return ConfigurationOperationResult::INVALID;
  }
  runtimeSynchronized_ = true;
  changed();
  return ConfigurationOperationResult::OK;
}

ConfigurationOperationResult ConfigurationService::discard(
    uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;
  const bool changedWspr =
      !sameWspr(manager_.candidateWspr(), manager_.savedWspr());
  const bool changedWifi =
      !sameWifi(manager_.candidateWifi(), manager_.savedWifi());
  manager_.discardCandidates();
  if (!changedWspr && !changedWifi) {
    return ConfigurationOperationResult::UNCHANGED;
  }
  changed();
  return ConfigurationOperationResult::OK;
}

ConfigurationOperationResult ConfigurationService::discardWspr(
    uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;
  if (sameWspr(manager_.candidateWspr(), manager_.savedWspr())) {
    return ConfigurationOperationResult::UNCHANGED;
  }
  manager_.discardWsprCandidate();
  changed();
  return ConfigurationOperationResult::OK;
}

ConfigurationOperationResult ConfigurationService::defaults(
    uint32_t expectedRevision) {
  if (stale(expectedRevision)) {
    return ConfigurationOperationResult::STALE_REVISION;
  }
  if (runtime_.busy()) return ConfigurationOperationResult::RF_BUSY;
  const bool changedWspr =
      !sameWspr(manager_.candidateWspr(), manager_.defaultWspr());
  const WifiCredentials emptyWifi;
  const bool changedWifi = !sameWifi(manager_.candidateWifi(), emptyWifi);
  manager_.loadDefaultsAsCandidates();
  if (!changedWspr && !changedWifi) {
    return ConfigurationOperationResult::UNCHANGED;
  }
  changed();
  return ConfigurationOperationResult::OK;
}

ConfigurationSaveOutcome ConfigurationService::save(
    uint32_t expectedRevision) {
  ConfigurationSaveOutcome outcome;
  if (stale(expectedRevision)) {
    outcome.result = ConfigurationOperationResult::STALE_REVISION;
    return outcome;
  }
  if (runtime_.busy()) {
    outcome.result = ConfigurationOperationResult::RF_BUSY;
    outcome.persistence = ConfigSaveResult::DEFERRED;
    return outcome;
  }
  const BeaconSettings previousActive = manager_.activeWspr();
  const ConfigurationStatus before = manager_.status();
  outcome.wsprChanged = before.wsprDirty;
  outcome.wifiChanged = before.wifiDirty;
  if (!manager_.requestSave()) {
    outcome.result = ConfigurationOperationResult::INVALID;
    outcome.persistence = ConfigSaveResult::INVALID;
    return outcome;
  }

  outcome.persistence = manager_.processPending(false);
  if (outcome.persistence == ConfigSaveResult::UNCHANGED) {
    outcome.result = ConfigurationOperationResult::UNCHANGED;
    return outcome;
  }
  if (outcome.persistence == ConfigSaveResult::PARTIAL) {
    changed();
    outcome.result = ConfigurationOperationResult::SAVE_PARTIAL;
    return outcome;
  }
  if (outcome.persistence != ConfigSaveResult::SAVED) {
    outcome.result = ConfigurationOperationResult::SAVE_FAILED;
    return outcome;
  }

  changed();
  if (outcome.wsprChanged && !runtime_.applyWspr(manager_.activeWspr())) {
    const bool rollbackApplied = runtime_.applyWspr(previousActive);
    manager_.restoreActiveWspr(previousActive);
    runtimeSynchronized_ = rollbackApplied;
    outcome.result = ConfigurationOperationResult::RUNTIME_APPLY_FAILED;
    return outcome;
  }
  if (outcome.wsprChanged) runtime_.applyPowerSave(manager_.activeWspr().powerSave);
  runtimeSynchronized_ = true;
  outcome.result = ConfigurationOperationResult::OK;
  return outcome;
}

WsprSaveOutcome ConfigurationService::saveWspr(
    uint32_t expectedRevision, WsprSaveSafetyContext &safety) {
  WsprSaveOutcome outcome;
  if (stale(expectedRevision)) {
    outcome.result = WsprSaveResult::STALE_REVISION;
    return outcome;
  }

  bool ownerAuthorized = false;
  if (safety.owner == ConfigurationSaveOwner::CLI) {
    ownerAuthorized = safety.sessions.cliActive();
  } else {
    ownerAuthorized =
        safety.sessions.authorizeWebMutation(
            safety.webSessionId, safety.nowMillis, safety.transmitter) ==
        ConfigurationSessionResult::OK;
  }
  if (!ownerAuthorized) {
    outcome.result = WsprSaveResult::CONFIG_MODE_REQUIRED;
    return outcome;
  }
  if (runtime_.busy() || !safety.transmitter.configurationSessionSafe() ||
      safety.transmitter.rfOutputEnabled() ||
      safety.transmitter.txAdmissionAllowed()) {
    outcome.result = WsprSaveResult::RF_UNSAFE;
    return outcome;
  }

  ConfigurationOperationGuard guard(safety.sessions, safety.transmitter);
  if (!guard.active() || runtime_.busy() ||
      !safety.transmitter.configurationSessionSafe() ||
      safety.transmitter.rfOutputEnabled() ||
      safety.transmitter.txAdmissionAllowed()) {
    outcome.result = WsprSaveResult::RF_UNSAFE;
    return outcome;
  }

  if (ConfigValidator::validate(manager_.candidateWspr()) !=
      ConfigValidation::VALID) {
    outcome.persistence = WsprPersistenceResult::INVALID;
    outcome.result = WsprSaveResult::INVALID_CONFIGURATION;
    return outcome;
  }

  const BeaconSettings previousActive = manager_.activeWspr();
  const BeaconSettings previousSaved = manager_.savedWspr();
  outcome.persistence = manager_.saveWsprOnly();
  if (outcome.persistence == WsprPersistenceResult::INVALID) {
    outcome.result = WsprSaveResult::INVALID_CONFIGURATION;
    return outcome;
  }
  if (outcome.persistence == WsprPersistenceResult::WRITE_FAILED) {
    outcome.result = WsprSaveResult::PERSISTENCE_FAILED;
    return outcome;
  }
  if (outcome.persistence == WsprPersistenceResult::VERIFICATION_FAILED) {
    outcome.result = WsprSaveResult::VERIFICATION_FAILED;
    return outcome;
  }

  outcome.persistenceOccurred =
      outcome.persistence == WsprPersistenceResult::SAVED;
  const BeaconSettings target = manager_.savedWspr();
  const bool activeNeedsReconciliation = !sameWspr(previousActive, target);
  if (!outcome.persistenceOccurred && !activeNeedsReconciliation &&
      runtimeSynchronized_) {
    outcome.runtimeApplySucceeded = true;
    outcome.result = WsprSaveResult::UNCHANGED;
    return outcome;
  }

  outcome.runtimeApplyAttempted = true;
  if (runtime_.applyWspr(target)) {
    runtime_.applyPowerSave(target.powerSave);
    manager_.restoreActiveWspr(target);
    runtimeSynchronized_ = true;
    outcome.runtimeApplySucceeded = true;
    if (outcome.persistenceOccurred || activeNeedsReconciliation) changed();
    outcome.result = WsprSaveResult::SAVED_APPLIED;
    return outcome;
  }

  outcome.rollbackSucceeded = runtime_.applyWspr(previousActive);
  if (outcome.rollbackSucceeded) runtime_.applyPowerSave(previousActive.powerSave);
  manager_.restoreActiveWspr(previousActive);
  runtimeSynchronized_ = outcome.rollbackSucceeded;
  if (!sameWspr(previousSaved, manager_.savedWspr())) changed();
  outcome.result = WsprSaveResult::PERSISTED_APPLY_FAILED;
  return outcome;
}
