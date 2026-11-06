#pragma once

#include <Arduino.h>

#include "NtpProviderCore.h"

struct NtpTimeSample {
  uint32_t utcMillisOfDay = 0;
  uint32_t referenceMonotonicMillis = 0;
};

class SntpProvider {
 public:
  explicit SntpProvider(const NtpProviderConfig &config) : core_(config) {}

  void begin(bool enabled, const char *primaryServer,
             const char *secondaryServer);
  void update(uint32_t monotonicMillis, bool wifiConnected);
  bool takeSample(NtpTimeSample &sample);
  NtpProviderSnapshot snapshot(uint32_t monotonicMillis) const {
    return core_.snapshot(monotonicMillis);
  }

 private:
  NtpProviderCore core_;
  const char *primaryServer_ = nullptr;
  const char *secondaryServer_ = nullptr;
  NtpTimeSample pendingSample_;
  bool samplePending_ = false;
  bool callbackInstalled_ = false;

  static volatile bool syncEventPending_;
  static void installSyncCallback();
  void startRequest(uint32_t monotonicMillis);
  void processSyncEvent(uint32_t monotonicMillis);
  void publishDiagnostics(uint32_t monotonicMillis);
};
