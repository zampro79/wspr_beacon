#include "TxTelemetry.h"

#include <string.h>

void TxTelemetry::prepareFrame(uint32_t slotUtcMillis, TxTimeSource source,
                               const char *callsign, const char *locator,
                               uint8_t powerDbm, uint32_t frequencyHz) {
  prepared_ = TxFrameSnapshot();
  prepared_.slotUtcMillis = slotUtcMillis;
  prepared_.frequencyHz = frequencyHz;
  prepared_.timeSource = source;
  if (callsign != nullptr) {
    strncpy(prepared_.callsign, callsign, sizeof(prepared_.callsign) - 1U);
    prepared_.callsign[sizeof(prepared_.callsign) - 1U] = '\0';
  }
  if (locator != nullptr) {
    strncpy(prepared_.locator, locator, sizeof(prepared_.locator) - 1U);
    prepared_.locator[4] = '\0';
  }
  prepared_.powerDbm = powerDbm;
  prepared_.valid = true;
}

void TxTelemetry::frameStarted() {
  if (!prepared_.valid) return;
  active_ = prepared_;
  snapshot_.framesStarted = incrementCounter(snapshot_.framesStarted);
  snapshot_.inhibitReason = TxInhibitReason::NONE;
}

void TxTelemetry::frameCompleted() {
  if (!active_.valid) return;
  active_.result = TxFrameResult::COMPLETED_BY_FIRMWARE;
  active_.endReason = TxInhibitReason::NONE;
  active_.rfVerified = false;
  snapshot_.lastFrame = active_;
  snapshot_.framesCompleted = incrementCounter(snapshot_.framesCompleted);
  active_ = TxFrameSnapshot();
  prepared_ = TxFrameSnapshot();
}

void TxTelemetry::frameInterrupted(TxInhibitReason reason) {
  if (!active_.valid) return;
  active_.result = reason == TxInhibitReason::MANUAL_TX_OFF
      ? TxFrameResult::INTERRUPTED_MANUAL_TX_OFF
      : TxFrameResult::INTERRUPTED_RF_ERROR;
  active_.endReason = reason;
  active_.rfVerified = false;
  snapshot_.lastFrame = active_;
  snapshot_.framesInterrupted = incrementCounter(snapshot_.framesInterrupted);
  snapshot_.inhibitReason = reason;
  active_ = TxFrameSnapshot();
  prepared_ = TxFrameSnapshot();
}
