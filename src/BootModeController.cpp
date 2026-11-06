#include "BootModeController.h"

void BootModeController::begin(bool usableSavedCredentials,
                               uint32_t nowMillis) {
  enterApRequested_ = false;
  apOperationalPending_ = false;
  if (usableSavedCredentials) {
    staStartedMillis_ = nowMillis;
    mode_ = SystemMode::STA_CONNECTING_INITIAL;
  } else {
    requestAp();
  }
}

ApOperationalRequestResult BootModeController::requestApOperational() {
  if (apOperationalPending_ && mode_ == SystemMode::PROVISIONING_AP) {
    return ApOperationalRequestResult::ALREADY_PENDING;
  }
  if (mode_ != SystemMode::PROVISIONING_AP) {
    return ApOperationalRequestResult::REJECTED;
  }
  apOperationalPending_ = true;
  return ApOperationalRequestResult::ACCEPTED;
}

bool BootModeController::completeApOperationalTransition() {
  if (!apOperationalPending_ || mode_ != SystemMode::PROVISIONING_AP) {
    return false;
  }
  apOperationalPending_ = false;
  mode_ = SystemMode::AP_OPERATIONAL;
  return true;
}

void BootModeController::failApOperationalTransition() {
  apOperationalPending_ = false;
  mode_ = SystemMode::PROVISIONING_ERROR;
}

void BootModeController::update(bool staConnected, bool staFailed,
                                uint32_t nowMillis) {
  if (mode_ != SystemMode::STA_CONNECTING_INITIAL) return;
  if (staConnected) {
    mode_ = SystemMode::NORMAL;
    return;
  }
  if (staFailed ||
      static_cast<uint32_t>(nowMillis - staStartedMillis_) >=
          INITIAL_STA_TIMEOUT_MS) {
    requestAp();
  }
}

void BootModeController::apStartResult(bool succeeded) {
  if (!enterApRequested_) return;
  enterApRequested_ = false;
  mode_ = succeeded ? SystemMode::PROVISIONING_AP
                    : SystemMode::PROVISIONING_ERROR;
}

void BootModeController::requestAp() {
  mode_ = SystemMode::BOOT_DECISION;
  enterApRequested_ = true;
}
