#include "NtpProviderCore.h"

NtpProviderCore::NtpProviderCore(const NtpProviderConfig &config)
    : config_(config) {}

void NtpProviderCore::begin(bool enabled) {
  enabled_ = enabled;
  wifiConnected_ = false;
  requestStarted_ = false;
  syncConfirmed_ = false;
  requestStartedMillis_ = 0;
  lastSyncMillis_ = 0;
  currentRecoveryDelayMillis_ = 0;
  nextRecoveryDelayMillis_ = config_.recoveryInitialMillis;
  recoveryDue_ = false;
  state_ = enabled ? NtpProviderState::STATE_WAITING_FOR_WIFI
                   : NtpProviderState::STATE_DISABLED;
}

void NtpProviderCore::setWifiConnected(bool connected) {
  if (!enabled_) return;
  if (wifiConnected_ && !connected && !syncConfirmed_) {
    requestStarted_ = false;
    recoveryDue_ = false;
    currentRecoveryDelayMillis_ = 0;
    nextRecoveryDelayMillis_ = config_.recoveryInitialMillis;
    state_ = NtpProviderState::STATE_WAITING_FOR_WIFI;
  }
  wifiConnected_ = connected;
}

bool NtpProviderCore::requestNeeded() const {
  return enabled_ && wifiConnected_ && (!requestStarted_ || recoveryDue_);
}

bool NtpProviderCore::markRequestStarted(uint32_t monotonicMillis) {
  if (!requestNeeded()) return false;
  requestStarted_ = true;
  syncConfirmed_ = false;
  recoveryDue_ = false;
  requestStartedMillis_ = monotonicMillis;
  currentRecoveryDelayMillis_ = nextRecoveryDelayMillis_;
  if (nextRecoveryDelayMillis_ != 0 &&
      config_.recoveryMaximumMillis > nextRecoveryDelayMillis_) {
    const uint32_t remaining =
        config_.recoveryMaximumMillis - nextRecoveryDelayMillis_;
    nextRecoveryDelayMillis_ +=
        nextRecoveryDelayMillis_ < remaining ? nextRecoveryDelayMillis_
                                             : remaining;
  }
  state_ = NtpProviderState::STATE_REQUESTING;
  return true;
}

void NtpProviderCore::markSynchronized(uint32_t monotonicMillis) {
  if (!enabled_ || !requestStarted_) return;
  syncConfirmed_ = true;
  recoveryDue_ = false;
  currentRecoveryDelayMillis_ = 0;
  nextRecoveryDelayMillis_ = config_.recoveryInitialMillis;
  lastSyncMillis_ = monotonicMillis;
  state_ = NtpProviderState::STATE_SYNCHRONIZED;
}

void NtpProviderCore::update(uint32_t monotonicMillis) {
  if (!enabled_) return;
  if (syncConfirmed_) {
    if (static_cast<uint32_t>(monotonicMillis - lastSyncMillis_) >
        config_.staleTimeoutMillis) {
      state_ = NtpProviderState::STATE_STALE;
      recoveryDue_ = config_.recoveryInitialMillis != 0;
    }
    return;
  }
  if (requestStarted_ &&
      static_cast<uint32_t>(monotonicMillis - requestStartedMillis_) >
          config_.requestTimeoutMillis) {
    state_ = NtpProviderState::STATE_TIMED_OUT;
  }
  if (state_ == NtpProviderState::STATE_TIMED_OUT &&
      currentRecoveryDelayMillis_ != 0 &&
      static_cast<uint32_t>(monotonicMillis - requestStartedMillis_) >=
          currentRecoveryDelayMillis_) {
    recoveryDue_ = true;
  }
}

NtpProviderSnapshot NtpProviderCore::snapshot(
    uint32_t monotonicMillis) const {
  NtpProviderSnapshot result;
  result.state = state_;
  result.wifiConnected = wifiConnected_;
  result.requestStarted = requestStarted_;
  result.syncConfirmed = syncConfirmed_;
  if (syncConfirmed_) {
    result.lastSyncAgeMillis =
        static_cast<uint32_t>(monotonicMillis - lastSyncMillis_);
  }
  return result;
}
