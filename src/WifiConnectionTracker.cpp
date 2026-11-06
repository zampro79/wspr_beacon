#include "WifiConnectionTracker.h"

void WifiConnectionTracker::beginConnecting(uint32_t monotonicMillis) {
  connectionStartedMillis_ = monotonicMillis;
  stateChangedMillis_ = monotonicMillis;
  state_ = WifiConnectionState::STATE_CONNECTING;
}

void WifiConnectionTracker::disable() {
  state_ = WifiConnectionState::STATE_DISABLED;
}

WifiConnectionState WifiConnectionTracker::update(uint32_t monotonicMillis,
                                                   bool linkConnected) {
  if (state_ == WifiConnectionState::STATE_DISABLED) return state_;
  if (linkConnected) {
    state_ = WifiConnectionState::STATE_CONNECTED;
    return state_;
  }
  if (state_ == WifiConnectionState::STATE_CONNECTED) {
    state_ = WifiConnectionState::STATE_DISCONNECTED;
    stateChangedMillis_ = monotonicMillis;
  } else if (state_ == WifiConnectionState::STATE_CONNECTING &&
             static_cast<uint32_t>(monotonicMillis -
                                   connectionStartedMillis_) > timeoutMillis_) {
    state_ = WifiConnectionState::STATE_FAILED;
    stateChangedMillis_ = monotonicMillis;
  }
  return state_;
}

bool WifiConnectionTracker::retryDue(uint32_t monotonicMillis,
                                     uint32_t retryDelayMillis) const {
  return (state_ == WifiConnectionState::STATE_DISCONNECTED ||
          state_ == WifiConnectionState::STATE_FAILED) &&
         static_cast<uint32_t>(monotonicMillis - stateChangedMillis_) >=
             retryDelayMillis;
}
