#include "WifiScanController.h"

#include <string.h>

#include "WifiCredentials.h"

WifiScanStartResult WifiScanController::start(WifiScanDriver &driver,
                                              uint32_t nowMillis) {
  if (snapshot_.state == WifiScanState::SCANNING) {
    return WifiScanStartResult::ALREADY_SCANNING;
  }
  if (snapshot_.driverBusy) return WifiScanStartResult::DRIVER_BUSY;
  if (snapshot_.recoveryBlocked) return WifiScanStartResult::DRIVER_BUSY;
  snapshot_.count = 0;
  snapshot_.completedTimestampValid = false;
  logicalTimedOut_ = false;
  recoveryPolls_ = 0;
  snapshot_.error = WifiScanError::NONE;
  snapshot_.apRestored = false;
  if (!driver.startAsync()) {
    snapshot_.state = WifiScanState::FAILED;
    snapshot_.error = WifiScanError::START_FAILED;
    return WifiScanStartResult::START_FAILED;
  }
  snapshot_.generation += 1U;
  snapshot_.state = WifiScanState::SCANNING;
  snapshot_.driverBusy = true;
  startedMillis_ = nowMillis;
  return WifiScanStartResult::STARTED;
}

void WifiScanController::update(WifiScanDriver &driver, uint32_t nowMillis) {
  if (!snapshot_.driverBusy || snapshot_.recoveryBlocked) return;
  if (logicalTimedOut_ &&
      (static_cast<uint32_t>(nowMillis - recoveryStartedMillis_) >=
           RECOVERY_TIMEOUT_MS || recoveryPolls_ >= MAX_RECOVERY_POLLS)) {
    snapshot_.recoveryBlocked = true;
    snapshot_.error = WifiScanError::RECOVERY_EXHAUSTED;
    return;
  }
  uint16_t count = 0;
  const WifiScanDriverStatus status = driver.poll(count);
  if (logicalTimedOut_) ++recoveryPolls_;
  if (status == WifiScanDriverStatus::RUNNING) {
    if (!logicalTimedOut_ &&
        static_cast<uint32_t>(nowMillis - startedMillis_) >= TIMEOUT_MS) {
      logicalTimedOut_ = true;
      recoveryStartedMillis_ = nowMillis;
      snapshot_.state = WifiScanState::FAILED;
      snapshot_.error = WifiScanError::TIMEOUT;
    }
    return;
  }
  if (status == WifiScanDriverStatus::FAILED) {
    // Arduino-ESP32 may report its wrapper timeout while the IDF callback is
    // still pending. Fail logically, retain the driver lock, and keep polling
    // until a late completion can be cleaned up safely.
    if (!logicalTimedOut_) recoveryStartedMillis_ = nowMillis;
    logicalTimedOut_ = true;
    snapshot_.state = WifiScanState::FAILED;
    snapshot_.error = WifiScanError::DRIVER_FAILED;
    return;
  }

  if (!logicalTimedOut_ &&
      static_cast<uint32_t>(nowMillis - startedMillis_) >= TIMEOUT_MS) {
    logicalTimedOut_ = true;
    snapshot_.state = WifiScanState::FAILED;
    snapshot_.error = WifiScanError::TIMEOUT;
  }
  if (!logicalTimedOut_) {
    copyResults(driver, count);
    snapshot_.state = WifiScanState::READY;
    snapshot_.completedMillis = nowMillis;
    snapshot_.completedTimestampValid = true;
  }
  driver.cleanup();
  snapshot_.driverBusy = false;
}

void WifiScanController::setApRestored(bool restored) {
  snapshot_.apRestored = restored;
}

void WifiScanController::blockApRestore() {
  snapshot_.state = WifiScanState::FAILED;
  snapshot_.apRestored = false;
  snapshot_.recoveryBlocked = true;
  snapshot_.error = WifiScanError::AP_RESTORE_FAILED;
}

bool WifiScanController::stale(uint32_t nowMillis) const {
  return snapshot_.state == WifiScanState::READY &&
         snapshot_.completedTimestampValid &&
         static_cast<uint32_t>(nowMillis - snapshot_.completedMillis) >=
             STALE_MS;
}

uint32_t WifiScanController::age(uint32_t nowMillis) const {
  return snapshot_.completedTimestampValid
      ? static_cast<uint32_t>(nowMillis - snapshot_.completedMillis) : 0U;
}

void WifiScanController::consume() {
  if (snapshot_.state == WifiScanState::READY ||
      (snapshot_.state == WifiScanState::FAILED && !snapshot_.driverBusy &&
       !snapshot_.recoveryBlocked)) {
    snapshot_.state = WifiScanState::IDLE;
  }
}

void WifiScanController::copyResults(WifiScanDriver &driver, uint16_t count) {
  snapshot_.count = 0;
  for (uint16_t index = 0; index < count; ++index) {
    WifiScanEntry entry;
    if (!driver.read(index, entry) ||
        !WifiCredentials::validSsid(entry.ssid)) continue;
    insert(entry);
  }
}

void WifiScanController::insert(const WifiScanEntry &entry) {
  for (uint8_t index = 0; index < snapshot_.count; ++index) {
    WifiScanEntry &existing = snapshot_.entries[index];
    if (existing.encrypted == entry.encrypted &&
        strcmp(existing.ssid, entry.ssid) == 0) {
      if (entry.rssi <= existing.rssi) return;
      existing.rssi = entry.rssi;
      while (index > 0 && snapshot_.entries[index].rssi >
                              snapshot_.entries[index - 1U].rssi) {
        const WifiScanEntry temporary = snapshot_.entries[index - 1U];
        snapshot_.entries[index - 1U] = snapshot_.entries[index];
        snapshot_.entries[index] = temporary;
        --index;
      }
      return;
    }
  }
  uint8_t position = 0;
  while (position < snapshot_.count &&
         snapshot_.entries[position].rssi >= entry.rssi) ++position;
  if (position >= MAX_RESULTS) return;
  const uint8_t newCount = snapshot_.count < MAX_RESULTS
      ? static_cast<uint8_t>(snapshot_.count + 1U) : snapshot_.count;
  for (uint8_t index = newCount - 1U; index > position; --index) {
    snapshot_.entries[index] = snapshot_.entries[index - 1U];
  }
  snapshot_.entries[position] = entry;
  snapshot_.count = newCount;
}
