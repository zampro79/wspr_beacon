#pragma once

#include <stdint.h>

enum class TxTimeSource : uint8_t { NONE, GPS, NTP };

enum class TxFrameResult : uint8_t {
  NONE,
  COMPLETED_BY_FIRMWARE,
  INTERRUPTED_RF_ERROR,
  INTERRUPTED_MANUAL_TX_OFF
};

enum class TxInhibitReason : uint8_t {
  NONE,
  RF_UNAVAILABLE,
  TIME_UNAVAILABLE,
  TIME_UNQUALIFIED,
  OPERATING_IDENTITY_UNCONFIGURED,
  MANUAL_TX_OFF,
  SLOT_MISSED,
  RF_RUNTIME_ERROR
};

struct TxFrameSnapshot {
  uint32_t slotUtcMillis = 0;
  uint32_t frequencyHz = 0;
  char callsign[7] = {};
  char locator[5] = {'-', '-', '-', '-', '\0'};
  uint8_t powerDbm = 0;
  TxTimeSource timeSource = TxTimeSource::NONE;
  TxFrameResult result = TxFrameResult::NONE;
  TxInhibitReason endReason = TxInhibitReason::NONE;
  bool valid = false;
  bool rfVerified = false;
};

struct TxTelemetrySnapshot {
  uint32_t framesStarted = 0;
  uint32_t framesCompleted = 0;
  uint32_t framesInterrupted = 0;
  TxInhibitReason inhibitReason = TxInhibitReason::TIME_UNAVAILABLE;
  TxFrameSnapshot lastFrame;
};

class TxTelemetry {
 public:
  void setInhibitReason(TxInhibitReason reason) { snapshot_.inhibitReason = reason; }
  void prepareFrame(uint32_t slotUtcMillis, TxTimeSource source,
                    const char *callsign, const char *locator,
                    uint8_t powerDbm, uint32_t frequencyHz);
  void frameStarted();
  void frameCompleted();
  void frameInterrupted(TxInhibitReason reason);
  const TxTelemetrySnapshot &snapshot() const { return snapshot_; }

  static uint32_t incrementCounter(uint32_t value) {
    return value == UINT32_MAX ? UINT32_MAX : value + 1U;
  }

 private:
  TxTelemetrySnapshot snapshot_;
  TxFrameSnapshot prepared_;
  TxFrameSnapshot active_;
};

static_assert(sizeof(TxTelemetrySnapshot) <= 48,
              "TX telemetry snapshot must remain compact");
