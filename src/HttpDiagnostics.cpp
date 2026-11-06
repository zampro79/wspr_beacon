#include "HttpDiagnostics.h"

#include <limits.h>

uint16_t HttpDiagnostics::bounded16(uint32_t value) {
  return value > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(value);
}

void HttpDiagnostics::increment(uint16_t &counter) {
  if (counter != UINT16_MAX) ++counter;
}

void HttpDiagnostics::observeGate(bool open, HttpGateCloseReason reason,
                                  uint32_t nowMillis) {
  snapshot_.flags |= (1U << 0);
  const bool changed = !gateObserved_ || snapshot_.gateOpen() != open;
  if (changed) {
    snapshot_.lastGateTransitionMillis = nowMillis;
    snapshot_.flags |= (1U << 3);
    if (open) increment(snapshot_.gateOpenings);
    else increment(snapshot_.gateClosures);
  }
  if (open) snapshot_.flags |= (1U << 1);
  else snapshot_.flags &= ~(1U << 1);
  snapshot_.gateReason = open ? HttpGateCloseReason::NONE : reason;
  gateObserved_ = true;
}

void HttpDiagnostics::listenerStarted() {
  snapshot_.flags |= (1U << 2);
}

void HttpDiagnostics::listenerStopped() {
  snapshot_.flags &= ~(1U << 2);
}

void HttpDiagnostics::clientAccepted() {
  increment(snapshot_.clientsAccepted);
}

void HttpDiagnostics::responseServed(uint16_t statusCode,
                                     uint32_t durationMillis, size_t bytes) {
  increment(snapshot_.responsesServed);
  snapshot_.maxResponseMillis =
      snapshot_.maxResponseMillis > bounded16(durationMillis)
          ? snapshot_.maxResponseMillis : bounded16(durationMillis);
  snapshot_.lastResponseMillis = bounded16(durationMillis);
  snapshot_.lastResponseBytes = bounded16(static_cast<uint32_t>(bytes));
  snapshot_.flags |= (1U << 4) | (1U << 5);
  if (statusCode >= 400U) increment(snapshot_.failedRequests);
}

void HttpDiagnostics::responseInterrupted(uint32_t durationMillis,
                                           size_t bytes) {
  increment(snapshot_.responsesInterrupted);
  const uint16_t duration = bounded16(durationMillis);
  if (duration > snapshot_.maxResponseMillis)
    snapshot_.maxResponseMillis = duration;
  snapshot_.lastResponseMillis = duration;
  snapshot_.lastResponseBytes = bounded16(static_cast<uint32_t>(bytes));
  snapshot_.flags |= (1U << 4);
  snapshot_.flags &= ~(1U << 5);
}

void HttpDiagnostics::recordWrite(size_t requested, size_t written,
                                  uint32_t durationMillis) {
  if (requested == 0) return;
  if (written == 0) increment(snapshot_.zeroWrites);
  else if (written < requested) increment(snapshot_.partialWrites);
  const uint32_t bytes = written > UINT32_MAX ? UINT32_MAX
                                             : static_cast<uint32_t>(written);
  snapshot_.bytesSent = bytes > UINT32_MAX - snapshot_.bytesSent
                            ? UINT32_MAX : snapshot_.bytesSent + bytes;
  const uint16_t duration = bounded16(durationMillis);
  if (duration > snapshot_.maxWriteMillis) snapshot_.maxWriteMillis = duration;
}

void HttpDiagnostics::noCapacityTurn() {
  increment(snapshot_.noCapacityTurns);
}

void HttpDiagnostics::clientClosed(HttpClientCloseReason reason) {
  if (reason == HttpClientCloseReason::GATE)
    increment(snapshot_.gateClientClosures);
  else if (reason == HttpClientCloseReason::DISCONNECT)
    increment(snapshot_.disconnectClosures);
}

void HttpDiagnostics::clientTimedOut(bool responsePhase) {
  increment(snapshot_.clientTimeouts);
  if (responsePhase) increment(snapshot_.responseTimeouts);
  else increment(snapshot_.requestTimeouts);
  increment(snapshot_.failedRequests);
}

void HttpDiagnostics::requestFailed() {
  increment(snapshot_.failedRequests);
}

void HttpDiagnostics::forcedClosure() {
  increment(snapshot_.forcedClosures);
}
