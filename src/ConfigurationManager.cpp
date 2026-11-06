#include "ConfigurationManager.h"
#include "ApSetupMarker.h"

#include <string.h>

#include "ConfigValidator.h"
#include "PersistentConfigRecord.h"

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
bool validWifi(const WifiCredentials &credentials) {
  const bool empty = !credentials.hasSsid() && !credentials.hasPassword() &&
                     !credentials.open;
  return empty || (WifiCredentials::validSsid(credentials.ssid) &&
                   (credentials.open ? !credentials.hasPassword()
                                     : WifiCredentials::validPassword(
                                           credentials.password)));
}

template <typename Value, typename Decoder>
bool loadLatest(ConfigStorage &storage, ConfigRecordType type, Decoder decode,
                Value &value, uint32_t &sequence, uint8_t &slot,
                bool &recordRejected) {
  bool found = false;
  uint8_t buffer[PersistentConfigRecord::MAX_RECORD_SIZE];
  for (uint8_t candidateSlot = 0; candidateSlot < 2; ++candidateSlot) {
    size_t length = 0;
    if (!storage.read(type, candidateSlot, buffer, sizeof(buffer), length)) continue;
    Value decoded;
    uint32_t decodedSequence = 0;
    if (!decode(buffer, length, decoded, decodedSequence)) {
      recordRejected = true;
      continue;
    }
    if (!found || PersistentConfigRecord::sequenceNewer(decodedSequence, sequence)) {
      value = decoded;
      sequence = decodedSequence;
      slot = candidateSlot;
      found = true;
    }
  }
  return found;
}

bool loadLatestWspr(ConfigStorage &storage, BeaconSettings &value,
                    uint32_t &sequence, uint8_t &slot,
                    uint8_t &payloadVersion, bool &recordRejected) {
  bool found = false;
  uint8_t buffer[PersistentConfigRecord::MAX_RECORD_SIZE];
  for (uint8_t candidateSlot = 0; candidateSlot < 2; ++candidateSlot) {
    size_t length = 0;
    if (!storage.read(ConfigRecordType::WSPR, candidateSlot, buffer,
                      sizeof(buffer), length)) {
      continue;
    }
    BeaconSettings decoded;
    uint32_t decodedSequence = 0;
    uint8_t decodedVersion = 0;
    if (!PersistentConfigRecord::decodeWsprWithVersion(
            buffer, length, decoded, decodedSequence, decodedVersion)) {
      recordRejected = true;
      continue;
    }
    if (!found ||
        PersistentConfigRecord::sequenceNewer(decodedSequence, sequence)) {
      value = decoded;
      sequence = decodedSequence;
      slot = candidateSlot;
      payloadVersion = decodedVersion;
      found = true;
    }
  }
  return found;
}
}  // namespace

ConfigurationManager::ConfigurationManager(
    ConfigStorage &storage, const BeaconSettings &defaultWspr)
    : storage_(storage), defaultWspr_(defaultWspr) {}

void ConfigurationManager::begin(bool factoryResetState,
                                 bool completedFactoryReset) {
  savedWspr_ = defaultWspr_;
  savedWifi_ = defaultWifi_;
  wsprSequence_ = wifiSequence_ = 0;
  wsprSlot_ = wifiSlot_ = 0;
  wsprPayloadVersion_ = 0;
  wsprOrigin_ = ConfigOrigin::DEFAULTS;
  wifiOrigin_ = ConfigOrigin::DEFAULTS;
  wsprRecordRejected_ = false;
  wifiRecordRejected_ = false;
  completedFactoryReset_ = false;
  if (factoryResetState) {
    enterFactoryResetState(completedFactoryReset);
    if (completedFactoryReset &&
        loadLatest(storage_, ConfigRecordType::WIFI,
                   PersistentConfigRecord::decodeWifi, savedWifi_,
                   wifiSequence_, wifiSlot_, wifiRecordRejected_)) {
      wifiOrigin_ = ConfigOrigin::SAVED;
      candidateWifi_ = activeWifi_ = savedWifi_;
    }
    savePending_ = false;
    lastSave_ = ConfigSaveResult::NONE;
    return;
  }
  if (loadLatestWspr(storage_, savedWspr_, wsprSequence_, wsprSlot_,
                     wsprPayloadVersion_, wsprRecordRejected_)) {
    wsprOrigin_ = ConfigOrigin::SAVED;
  }
  if (loadLatest(storage_, ConfigRecordType::WIFI,
                 PersistentConfigRecord::decodeWifi, savedWifi_, wifiSequence_,
                 wifiSlot_, wifiRecordRejected_)) {
    wifiOrigin_ = ConfigOrigin::SAVED;
  }
  candidateWspr_ = activeWspr_ = savedWspr_;
  candidateWifi_ = activeWifi_ = savedWifi_;
  savePending_ = false;
  lastSave_ = ConfigSaveResult::NONE;
}

void ConfigurationManager::enterFactoryResetState(bool transactionCompleted) {
  BeaconSettings reset = defaultWspr_;
  reset.callsign[0] = '\0';
  strcpy(reset.fixedLocator, WSPR_DEFAULT_LOCATOR);
  reset.band = BandPlan::DEFAULT_BAND;
  reset.frequencyMode = WsprFrequencyMode::FIXED;
  reset.powerSave = false;
  reset.baseFrequencyHz = BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ;
  reset.frequencyOffsetHz = 0;
  savedWspr_ = candidateWspr_ = activeWspr_ = reset;
  savedWifi_ = candidateWifi_ = activeWifi_ = defaultWifi_;
  wsprOrigin_ = wifiOrigin_ = ConfigOrigin::DEFAULTS;
  wsprSequence_ = wifiSequence_ = 0;
  wsprSlot_ = wifiSlot_ = 0;
  wsprPayloadVersion_ = 0;
  savePending_ = false;
  lastSave_ = ConfigSaveResult::NONE;
  completedFactoryReset_ = transactionCompleted;
}

bool ConfigurationManager::setWsprCandidate(const BeaconSettings &settings) {
  if (ConfigValidator::validate(settings) != ConfigValidation::VALID) return false;
  candidateWspr_ = settings;
  return true;
}
bool ConfigurationManager::applyWsprSelection(
    WsprBandId band, WsprFrequencyMode mode) {
  BeaconSettings proposed = activeWspr_;
  proposed.band = band;
  proposed.frequencyMode = mode;
  if (ConfigValidator::validate(proposed) != ConfigValidation::VALID) {
    return false;
  }
  activeWspr_ = proposed;
  return true;
}
bool ConfigurationManager::setWifiCandidate(const WifiCredentials &credentials) {
  if (!validWifi(credentials)) return false;
  candidateWifi_ = credentials;
  return true;
}
void ConfigurationManager::discardCandidates() {
  candidateWspr_ = savedWspr_;
  candidateWifi_ = savedWifi_;
  savePending_ = false;
}

void ConfigurationManager::discardWsprCandidate() {
  candidateWspr_ = savedWspr_;
}
void ConfigurationManager::loadDefaultsAsCandidates() {
  candidateWspr_ = defaultWspr_;
  candidateWifi_ = defaultWifi_;
  savePending_ = false;
}
bool ConfigurationManager::requestSave() {
  if (ConfigValidator::validate(candidateWspr_) != ConfigValidation::VALID ||
      !validWifi(candidateWifi_)) {
    lastSave_ = ConfigSaveResult::INVALID;
    return false;
  }
  savePending_ = true;
  lastSave_ = ConfigSaveResult::DEFERRED;
  return true;
}

ConfigSaveResult ConfigurationManager::processPending(bool txBusy) {
  if (!savePending_) return lastSave_;
  if (txBusy) return lastSave_ = ConfigSaveResult::DEFERRED;
  const bool writeWspr = wsprDirty() || wsprMigrationPending();
  const bool writeWifi = wifiDirty();
  if (!writeWspr && !writeWifi) {
    savePending_ = false;
    return lastSave_ = ConfigSaveResult::UNCHANGED;
  }
  const WsprPersistenceResult wsprResult =
      writeWspr ? saveWspr() : WsprPersistenceResult::UNCHANGED;
  const bool wsprSaved = wsprResult == WsprPersistenceResult::SAVED ||
                         wsprResult == WsprPersistenceResult::UNCHANGED;
  const WifiPersistenceResult wifiResult =
      writeWifi ? saveWifi() : WifiPersistenceResult::UNCHANGED;
  const bool wifiSaved = wifiResult == WifiPersistenceResult::SAVED ||
                         wifiResult == WifiPersistenceResult::UNCHANGED;
  savePending_ = false;
  if (wsprSaved && wifiSaved) {
    if (writeWspr) activeWspr_ = candidateWspr_;
    if (writeWifi) activeWifi_ = candidateWifi_;
    return lastSave_ = ConfigSaveResult::SAVED;
  }
  if ((writeWspr && wsprSaved) || (writeWifi && wifiSaved)) {
    return lastSave_ = ConfigSaveResult::PARTIAL;
  }
  return lastSave_ = ConfigSaveResult::FAILED;
}

bool ConfigurationManager::restoreActiveWspr(const BeaconSettings &settings) {
  BeaconSettings validationCopy = settings;
  if (validationCopy.callsign[0] == '\0') {
    strcpy(validationCopy.callsign, "AA0AA");
  }
  if (ConfigValidator::validate(validationCopy) != ConfigValidation::VALID) {
    return false;
  }
  activeWspr_ = settings;
  return true;
}

WsprPersistenceResult ConfigurationManager::saveWsprOnly() {
  if (ConfigValidator::validate(candidateWspr_) != ConfigValidation::VALID) {
    lastSave_ = ConfigSaveResult::INVALID;
    return WsprPersistenceResult::INVALID;
  }
  if (!wsprDirty() && !wsprMigrationPending()) {
    lastSave_ = ConfigSaveResult::UNCHANGED;
    return WsprPersistenceResult::UNCHANGED;
  }
  return saveWspr();
}

WsprPersistenceResult ConfigurationManager::saveWspr() {
  uint8_t encoded[PersistentConfigRecord::HEADER_SIZE +
                  PersistentConfigRecord::WSPR_PAYLOAD_SIZE];
  size_t length = 0;
  const uint32_t sequence = wsprSequence_ + 1;
  const uint8_t slot = static_cast<uint8_t>(wsprSlot_ ^ 1U);
  if (!PersistentConfigRecord::encodeWspr(candidateWspr_, sequence, encoded,
                                          sizeof(encoded), length) ||
      !storage_.write(ConfigRecordType::WSPR, slot, encoded, length)) {
    lastSave_ = ConfigSaveResult::FAILED;
    return WsprPersistenceResult::WRITE_FAILED;
  }
  uint8_t verify[sizeof(encoded)];
  size_t verifyLength = 0;
  BeaconSettings decoded;
  uint32_t decodedSequence = 0;
  uint8_t decodedVersion = 0;
  if (!storage_.read(ConfigRecordType::WSPR, slot, verify, sizeof(verify),
                     verifyLength) ||
      !PersistentConfigRecord::decodeWsprWithVersion(
          verify, verifyLength, decoded, decodedSequence, decodedVersion) ||
      decodedVersion != PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V3 ||
      decodedSequence != sequence || !sameWspr(decoded, candidateWspr_)) {
    lastSave_ = ConfigSaveResult::FAILED;
    return WsprPersistenceResult::VERIFICATION_FAILED;
  }
  if (completedFactoryReset_) {
    // Do not release the Factory Reset marker while AP Setup could still
    // force provisioning on the next boot.
    ApSetupMarker apSetup(storage_);
    if (apSetup.cancelForFactoryReset() != ApSetupMarkerResult::OK) {
      lastSave_ = ConfigSaveResult::FAILED;
      return WsprPersistenceResult::VERIFICATION_FAILED;
    }
    if (!storage_.clearFactoryMarkers()) {
      lastSave_ = ConfigSaveResult::FAILED;
      return WsprPersistenceResult::VERIFICATION_FAILED;
    }
    for (uint8_t markerSlot = 0; markerSlot < 2; ++markerSlot) {
      uint8_t marker[16];
      size_t markerLength = 0;
      if (storage_.readFactoryMarker(markerSlot, marker, sizeof(marker),
                                     markerLength) !=
          StorageReadResult::ABSENT) {
        lastSave_ = ConfigSaveResult::FAILED;
        return WsprPersistenceResult::VERIFICATION_FAILED;
      }
    }
  }
  savedWspr_ = decoded;
  wsprSequence_ = sequence;
  wsprSlot_ = slot;
  wsprPayloadVersion_ = PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V3;
  wsprOrigin_ = ConfigOrigin::SAVED;
  completedFactoryReset_ = false;
  lastSave_ = ConfigSaveResult::SAVED;
  return WsprPersistenceResult::SAVED;
}

WifiPersistenceResult ConfigurationManager::saveWifiOnly(
    const WifiCredentials &credentials) {
  if (!validWifi(credentials) || !credentials.complete()) {
    return WifiPersistenceResult::INVALID;
  }
  if (sameWifi(credentials, savedWifi_)) {
    return WifiPersistenceResult::UNCHANGED;
  }
  const WifiCredentials previousCandidate = candidateWifi_;
  candidateWifi_ = credentials;
  const WifiPersistenceResult result = saveWifi();
  candidateWifi_ = previousCandidate;
  return result;
}

WifiPersistenceResult ConfigurationManager::saveWifi() {
  uint8_t encoded[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  const uint32_t sequence = wifiSequence_ + 1;
  const uint8_t slot = static_cast<uint8_t>(wifiSlot_ ^ 1U);
  if (!PersistentConfigRecord::encodeWifi(candidateWifi_, sequence, encoded,
                                          sizeof(encoded), length) ||
      !storage_.write(ConfigRecordType::WIFI, slot, encoded, length)) {
    return WifiPersistenceResult::WRITE_FAILED;
  }
  uint8_t verify[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t verifyLength = 0;
  WifiCredentials decoded;
  uint32_t decodedSequence = 0;
  if (!storage_.read(ConfigRecordType::WIFI, slot, verify, sizeof(verify),
                     verifyLength) ||
      !PersistentConfigRecord::decodeWifi(verify, verifyLength, decoded,
                                          decodedSequence) ||
      decodedSequence != sequence || !sameWifi(decoded, candidateWifi_)) {
    return WifiPersistenceResult::VERIFICATION_FAILED;
  }
  savedWifi_ = decoded;
  wifiSequence_ = sequence;
  wifiSlot_ = slot;
  wifiOrigin_ = ConfigOrigin::SAVED;
  return WifiPersistenceResult::SAVED;
}

bool ConfigurationManager::wsprDirty() const {
  return !sameWspr(candidateWspr_, savedWspr_);
}
bool ConfigurationManager::wsprMigrationPending() const {
  return wsprPayloadVersion_ != 0 &&
         wsprPayloadVersion_ < PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V3;
}
bool ConfigurationManager::wifiDirty() const {
  return !sameWifi(candidateWifi_, savedWifi_);
}
ConfigurationStatus ConfigurationManager::status() const {
  ConfigurationStatus result;
  result.wsprOrigin = wsprOrigin_;
  result.wifiOrigin = wifiOrigin_;
  result.lastSave = lastSave_;
  result.savePending = savePending_;
  result.wsprDirty = wsprDirty();
  result.wifiDirty = wifiDirty();
  result.wsprRecordRejected = wsprRecordRejected_;
  result.wifiRecordRejected = wifiRecordRejected_;
  result.wsprMigrationPending = wsprMigrationPending();
  result.wsprPayloadVersion = wsprPayloadVersion_;
  return result;
}
