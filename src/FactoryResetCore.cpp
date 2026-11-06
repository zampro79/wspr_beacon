#include "FactoryResetCore.h"
#include "ApSetupMarker.h"

#include <string.h>

#include "ConfigurationManager.h"
#include "ManualTxController.h"
#include "PersistentConfigRecord.h"
#include "WsprTransmitter.h"

namespace {
constexpr uint8_t MAGIC[4] = {'F', 'R', 'S', 'T'};
constexpr uint8_t FORMAT_VERSION = 1;
constexpr uint8_t STATE_IN_PROGRESS = 1;
constexpr uint8_t STATE_COMPLETE = 2;

void put32(uint8_t *data, uint32_t value) {
  for (uint8_t index = 0; index < 4; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8));
  }
}
uint32_t get32(const uint8_t *data) {
  uint32_t value = 0;
  for (uint8_t index = 0; index < 4; ++index) {
    value |= static_cast<uint32_t>(data[index]) << (index * 8);
  }
  return value;
}
bool sequenceNewer(uint32_t candidate, uint32_t reference) {
  return static_cast<int32_t>(candidate - reference) > 0;
}
bool allZero(const uint8_t *data, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    if (data[index] != 0) return false;
  }
  return true;
}
}  // namespace

FactoryResetCore::MarkerSnapshot FactoryResetCore::readMarker() const {
  MarkerSnapshot selected;
  bool found = false;
  bool corrupt = false;
  for (uint8_t slot = 0; slot < 2; ++slot) {
    uint8_t data[MARKER_SIZE];
    size_t length = 0;
    const StorageReadResult result =
        storage_.readFactoryMarker(slot, data, sizeof(data), length);
    if (result == StorageReadResult::ERROR) {
      selected.storageError = true;
      return selected;
    }
    if (result == StorageReadResult::ABSENT) continue;
    if (length != MARKER_SIZE || memcmp(data, MAGIC, sizeof(MAGIC)) != 0 ||
        data[4] != FORMAT_VERSION || data[6] != 0 || data[7] != 0 ||
        get32(data + 12) != PersistentConfigRecord::crc32(data, 12) ||
        (data[5] != STATE_IN_PROGRESS && data[5] != STATE_COMPLETE)) {
      corrupt = true;
      continue;
    }
    const uint32_t sequence = get32(data + 8);
    if (!found || sequenceNewer(sequence, selected.sequence)) {
      selected.state = data[5] == STATE_IN_PROGRESS
                           ? MarkerState::IN_PROGRESS
                           : MarkerState::COMPLETE;
      selected.sequence = sequence;
      selected.slot = slot;
      found = true;
    }
  }
  if (!found && corrupt) selected.state = MarkerState::INVALID;
  return selected;
}

bool FactoryResetCore::writeMarker(MarkerState state,
                                   const MarkerSnapshot &previous,
                                   MarkerSnapshot &written) {
  uint8_t data[MARKER_SIZE] = {};
  memcpy(data, MAGIC, sizeof(MAGIC));
  data[4] = FORMAT_VERSION;
  data[5] = state == MarkerState::IN_PROGRESS ? STATE_IN_PROGRESS
                                              : STATE_COMPLETE;
  const uint32_t sequence = previous.sequence + 1U;
  put32(data + 8, sequence);
  put32(data + 12, PersistentConfigRecord::crc32(data, 12));
  const uint8_t slot = previous.state == MarkerState::NONE ||
                               previous.state == MarkerState::INVALID
                           ? 0
                           : static_cast<uint8_t>(previous.slot ^ 1U);
  if (!storage_.writeFactoryMarker(slot, data, sizeof(data))) return false;
  uint8_t verify[MARKER_SIZE];
  size_t verifyLength = 0;
  if (storage_.readFactoryMarker(slot, verify, sizeof(verify), verifyLength) !=
          StorageReadResult::OK ||
      verifyLength != sizeof(data) || memcmp(verify, data, sizeof(data)) != 0) {
    return false;
  }
  written.state = state;
  written.sequence = sequence;
  written.slot = slot;
  return true;
}

bool FactoryResetCore::invalidateAndVerify(ConfigRecordType type,
                                           uint8_t slot) {
  if (!storage_.invalidate(type, slot)) return false;
  uint8_t verify[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  const size_t expected = type == ConfigRecordType::WSPR
                              ? PersistentConfigRecord::WSPR_RECORD_SIZE
                              : PersistentConfigRecord::WIFI_RECORD_SIZE;
  return storage_.read(type, slot, verify, sizeof(verify), length) &&
         length == expected && allZero(verify, length);
}

FactoryResetResult FactoryResetCore::finish(const MarkerSnapshot &pending) {
  for (uint8_t slot = 0; slot < 2; ++slot) {
    if (!invalidateAndVerify(ConfigRecordType::WSPR, slot)) {
      return FactoryResetResult::VERIFICATION_FAILED;
    }
  }
  for (uint8_t slot = 0; slot < 2; ++slot) {
    if (!invalidateAndVerify(ConfigRecordType::WIFI, slot)) {
      return FactoryResetResult::VERIFICATION_FAILED;
    }
  }
  MarkerSnapshot complete;
  if (!writeMarker(MarkerState::COMPLETE, pending, complete)) {
    return FactoryResetResult::VERIFICATION_FAILED;
  }
  bootState_ = FactoryResetBootState::COMPLETED;
  return FactoryResetResult::COMPLETED;
}

FactoryResetBootState FactoryResetCore::recover(ManualTxController &manualTx) {
  const MarkerSnapshot marker = readMarker();
  if (marker.storageError || marker.state == MarkerState::INVALID) {
    manualTx.turnOff();
    return bootState_ = FactoryResetBootState::FAIL_CLOSED;
  }
  if (marker.state == MarkerState::NONE) {
    return bootState_ = FactoryResetBootState::NORMAL;
  }
  manualTx.turnOff();
  // The factory marker is already durable. Keep it until AP Setup is
  // cancelled and verified, including recovery from an interrupted reset.
  ApSetupMarker apSetup(storage_);
  if (apSetup.cancelForFactoryReset() != ApSetupMarkerResult::OK) {
    return bootState_ = FactoryResetBootState::FAIL_CLOSED;
  }
  if (marker.state == MarkerState::COMPLETE) {
    return bootState_ = FactoryResetBootState::COMPLETED;
  }
  if (finish(marker) != FactoryResetResult::COMPLETED) {
    return bootState_ = FactoryResetBootState::FAIL_CLOSED;
  }
  return bootState_ = FactoryResetBootState::RECOVERED;
}

FactoryResetResult FactoryResetCore::reset(
    ManualTxController &manualTx, ConfigurationManager &configuration,
    WsprTransmitter &transmitter) {
  manualTx.turnOff();
  const MarkerSnapshot current = readMarker();
  if (current.storageError) {
    bootState_ = FactoryResetBootState::FAIL_CLOSED;
    return FactoryResetResult::STORAGE_FAILED;
  }
  if (current.state == MarkerState::COMPLETE) {
    configuration.enterFactoryResetState(true);
    if (!transmitter.setOperatingIdentityUnconfigured(
            configuration.activeWspr())) {
      bootState_ = FactoryResetBootState::FAIL_CLOSED;
      return FactoryResetResult::RUNTIME_FAILED;
    }
    ApSetupMarker apSetup(storage_);
    if (apSetup.cancelForFactoryReset() != ApSetupMarkerResult::OK) {
      bootState_ = FactoryResetBootState::FAIL_CLOSED;
      return FactoryResetResult::VERIFICATION_FAILED;
    }
    bootState_ = FactoryResetBootState::COMPLETED;
    return FactoryResetResult::ALREADY_COMPLETED;
  }
  MarkerSnapshot pending;
  if (!writeMarker(MarkerState::IN_PROGRESS, current, pending)) {
    bootState_ = FactoryResetBootState::FAIL_CLOSED;
    return FactoryResetResult::VERIFICATION_FAILED;
  }
  configuration.enterFactoryResetState(false);
  if (!transmitter.setOperatingIdentityUnconfigured(
          configuration.activeWspr())) {
    bootState_ = FactoryResetBootState::FAIL_CLOSED;
    return FactoryResetResult::RUNTIME_FAILED;
  }
  ApSetupMarker apSetup(storage_);
  if (apSetup.cancelForFactoryReset() != ApSetupMarkerResult::OK) {
    bootState_ = FactoryResetBootState::FAIL_CLOSED;
    return FactoryResetResult::VERIFICATION_FAILED;
  }
  const FactoryResetResult result = finish(pending);
  if (result != FactoryResetResult::COMPLETED) {
    bootState_ = FactoryResetBootState::FAIL_CLOSED;
  } else {
    configuration.enterFactoryResetState(true);
  }
  return result;
}
