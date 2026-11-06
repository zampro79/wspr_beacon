#pragma once

#include <stdint.h>

enum class WifiConnectionState : uint8_t {
  STATE_DISABLED,
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_FAILED,
  STATE_AP_OPERATIONAL
};

class WifiConnectionTracker {
 public:
  explicit WifiConnectionTracker(uint32_t timeoutMillis)
      : timeoutMillis_(timeoutMillis) {}

  void beginConnecting(uint32_t monotonicMillis);
  void disable();
  WifiConnectionState update(uint32_t monotonicMillis, bool linkConnected);
  bool retryDue(uint32_t monotonicMillis, uint32_t retryDelayMillis) const;
  WifiConnectionState state() const { return state_; }
  bool connected() const { return state_ == WifiConnectionState::STATE_CONNECTED; }

 private:
  uint32_t timeoutMillis_;
  uint32_t connectionStartedMillis_ = 0;
  uint32_t stateChangedMillis_ = 0;
  WifiConnectionState state_ = WifiConnectionState::STATE_DISABLED;
};
