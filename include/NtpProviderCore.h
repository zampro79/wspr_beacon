#pragma once

#include <stdint.h>

enum class NtpProviderState : uint8_t {
  STATE_DISABLED,
  STATE_WAITING_FOR_WIFI,
  STATE_REQUESTING,
  STATE_SYNCHRONIZED,
  STATE_TIMED_OUT,
  STATE_STALE
};

struct NtpProviderConfig {
  constexpr NtpProviderConfig(uint32_t requestTimeout = 0,
                              uint32_t staleTimeout = 0,
                              uint32_t recoveryInitial = 0,
                              uint32_t recoveryMaximum = 0)
      : requestTimeoutMillis(requestTimeout),
        staleTimeoutMillis(staleTimeout),
        recoveryInitialMillis(recoveryInitial),
        recoveryMaximumMillis(recoveryMaximum) {}

  uint32_t requestTimeoutMillis;
  uint32_t staleTimeoutMillis;
  uint32_t recoveryInitialMillis;
  uint32_t recoveryMaximumMillis;
};

struct NtpProviderSnapshot {
  NtpProviderState state = NtpProviderState::STATE_DISABLED;
  uint32_t lastSyncAgeMillis = UINT32_MAX;
  bool wifiConnected = false;
  bool requestStarted = false;
  bool syncConfirmed = false;
};

class NtpProviderCore {
 public:
  explicit NtpProviderCore(const NtpProviderConfig &config);

  void begin(bool enabled);
  void setWifiConnected(bool connected);
  bool requestNeeded() const;
  bool markRequestStarted(uint32_t monotonicMillis);
  void markSynchronized(uint32_t monotonicMillis);
  void update(uint32_t monotonicMillis);
  NtpProviderSnapshot snapshot(uint32_t monotonicMillis) const;

 private:
  NtpProviderConfig config_;
  NtpProviderState state_ = NtpProviderState::STATE_DISABLED;
  uint32_t requestStartedMillis_ = 0;
  uint32_t lastSyncMillis_ = 0;
  uint32_t currentRecoveryDelayMillis_ = 0;
  uint32_t nextRecoveryDelayMillis_ = 0;
  bool enabled_ = false;
  bool wifiConnected_ = false;
  bool requestStarted_ = false;
  bool syncConfirmed_ = false;
  bool recoveryDue_ = false;
};

static_assert(sizeof(NtpProviderSnapshot) <= 12,
              "NtpProviderSnapshot must remain compact");
