#include "SntpProvider.h"

#include <time.h>

#include "BoardConfig.h"
#include "DiagnosticRegistry.h"
#include "NtpSettings.h"

#if WSPR_HAS_WIFI
  #if defined(ARDUINO_ARCH_ESP8266)
    #include <coredecls.h>
  #elif defined(ARDUINO_ARCH_ESP32)
    #include <esp_sntp.h>
  #endif
#endif

namespace {
constexpr time_t MINIMUM_VALID_EPOCH = 1577836800;  // 2020-01-01 UTC
constexpr uint32_t SECONDS_PER_DAY = 86400UL;
}  // namespace

volatile bool SntpProvider::syncEventPending_ = false;

void SntpProvider::begin(bool enabled, const char *primaryServer,
                         const char *secondaryServer) {
  primaryServer_ = primaryServer;
  secondaryServer_ = secondaryServer;
  core_.begin(enabled);
  publishDiagnostics(millis());
}

void SntpProvider::installSyncCallback() {
#if defined(ARDUINO_ARCH_ESP8266)
  settimeofday_cb([](bool fromSntp) {
    if (fromSntp) syncEventPending_ = true;
  });
#elif defined(ARDUINO_ARCH_ESP32)
  sntp_set_time_sync_notification_cb([](struct timeval *) {
    syncEventPending_ = true;
  });
#endif
}

void SntpProvider::startRequest(uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  if (!callbackInstalled_) {
    installSyncCallback();
    callbackInstalled_ = true;
  }
  configTime(0, 0, primaryServer_, secondaryServer_);
#if defined(ARDUINO_ARCH_ESP32)
  sntp_set_sync_interval(ESP32_SNTP_SYNC_INTERVAL_MS);
  sntp_restart();
#endif
  core_.markRequestStarted(monotonicMillis);
#else
  (void)monotonicMillis;
#endif
}

void SntpProvider::processSyncEvent(uint32_t monotonicMillis) {
  if (!syncEventPending_) return;
  syncEventPending_ = false;
  const time_t epoch = time(nullptr);
  if (epoch < MINIMUM_VALID_EPOCH) return;

  pendingSample_.utcMillisOfDay =
      static_cast<uint32_t>(epoch % SECONDS_PER_DAY) * 1000UL;
  pendingSample_.referenceMonotonicMillis = monotonicMillis;
  samplePending_ = true;
  core_.markSynchronized(monotonicMillis);
  diagnostics.recordSuccess(DiagnosticModule::NTP);
}

void SntpProvider::update(uint32_t monotonicMillis, bool wifiConnected) {
  core_.setWifiConnected(wifiConnected);
  processSyncEvent(monotonicMillis);
  core_.update(monotonicMillis);
  if (core_.requestNeeded()) startRequest(monotonicMillis);
  publishDiagnostics(monotonicMillis);
}

bool SntpProvider::takeSample(NtpTimeSample &sample) {
  if (!samplePending_) return false;
  sample = pendingSample_;
  samplePending_ = false;
  return true;
}

void SntpProvider::publishDiagnostics(uint32_t monotonicMillis) {
  const NtpProviderState state = core_.snapshot(monotonicMillis).state;
  DiagnosticState diagnosticState = DiagnosticState::STATE_UNKNOWN;
  DiagnosticReason reason = DiagnosticReason::NOT_EVALUATED;
  switch (state) {
    case NtpProviderState::STATE_DISABLED:
      diagnosticState = DiagnosticState::STATE_DISABLED;
      reason = DiagnosticReason::DISABLED_BY_CONFIGURATION;
      break;
    case NtpProviderState::STATE_WAITING_FOR_WIFI:
      diagnosticState = DiagnosticState::STATE_INITIALIZING;
      reason = DiagnosticReason::SERVICE_UNAVAILABLE;
      break;
    case NtpProviderState::STATE_REQUESTING:
      diagnosticState = DiagnosticState::STATE_INITIALIZING;
      reason = DiagnosticReason::WAITING_FOR_DATA;
      break;
    case NtpProviderState::STATE_SYNCHRONIZED:
      diagnosticState = DiagnosticState::STATE_OPERATIONAL;
      reason = DiagnosticReason::NONE;
      break;
    case NtpProviderState::STATE_TIMED_OUT:
    case NtpProviderState::STATE_STALE:
      diagnosticState = DiagnosticState::STATE_DEGRADED;
      reason = DiagnosticReason::TIMEOUT;
      break;
  }
  diagnostics.setState(DiagnosticModule::NTP, diagnosticState, reason,
                       monotonicMillis);
}
