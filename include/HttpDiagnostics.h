#pragma once

#include <stdint.h>
#include <stddef.h>

enum class HttpClientCloseReason : uint8_t { COMPLETE, GATE, TIMEOUT, DISCONNECT };

enum class HttpGateCloseReason : uint8_t {
  NONE,
  WIFI_DISCONNECTED,
  RF_CRITICAL,
  SLOT_GUARD,
  TIME_UNCERTAIN
};

struct HttpDiagnosticsSnapshot {
  uint32_t lastGateTransitionMillis = 0;
  uint16_t gateOpenings = 0;
  uint16_t gateClosures = 0;
  uint16_t clientsAccepted = 0;
  uint16_t responsesServed = 0;
  uint16_t clientTimeouts = 0;
  uint16_t requestTimeouts = 0;
  uint16_t responseTimeouts = 0;
  uint16_t failedRequests = 0;
  uint16_t forcedClosures = 0;
  uint32_t bytesSent = 0;
  uint16_t responsesInterrupted = 0;
  uint16_t partialWrites = 0;
  uint16_t zeroWrites = 0;
  uint16_t noCapacityTurns = 0;
  uint16_t maxWriteMillis = 0;
  uint16_t maxResponseMillis = 0;
  uint16_t lastResponseMillis = 0;
  uint16_t lastResponseBytes = 0;
  uint16_t gateClientClosures = 0;
  uint16_t disconnectClosures = 0;
  HttpGateCloseReason gateReason = HttpGateCloseReason::NONE;
  uint8_t flags = 0;

  bool available() const { return (flags & (1U << 0)) != 0; }
  bool gateOpen() const { return (flags & (1U << 1)) != 0; }
  bool listenerIntended() const { return (flags & (1U << 2)) != 0; }
  bool gateTransitionValid() const { return (flags & (1U << 3)) != 0; }
  bool lastResponseValid() const { return (flags & (1U << 4)) != 0; }
  bool lastResponseComplete() const { return (flags & (1U << 5)) != 0; }
  bool gateAgeMillis(uint32_t observationMillis, uint32_t &ageMillis) const {
    if (!gateTransitionValid()) return false;
    const uint32_t elapsed = observationMillis - lastGateTransitionMillis;
    // Wraparound produces a small elapsed value. A transition sampled after
    // the observation produces a value in the opposite half of the range.
    if (elapsed > 0x7FFFFFFFUL) return false;
    ageMillis = elapsed;
    return true;
  }
};

class HttpDiagnostics {
 public:
  void observeGate(bool open, HttpGateCloseReason reason, uint32_t nowMillis);
  void listenerStarted();
  void listenerStopped();
  void clientAccepted();
  void responseServed(uint16_t statusCode, uint32_t durationMillis = 0,
                      size_t bytes = 0);
  void responseInterrupted(uint32_t durationMillis, size_t bytes);
  void recordWrite(size_t requested, size_t written, uint32_t durationMillis);
  void noCapacityTurn();
  void clientClosed(HttpClientCloseReason reason);
  void clientTimedOut(bool responsePhase = false);
  void requestFailed();
  void forcedClosure();
  const HttpDiagnosticsSnapshot &snapshot() const { return snapshot_; }

 private:
  HttpDiagnosticsSnapshot snapshot_;
  bool gateObserved_ = false;

  static void increment(uint16_t &counter);
  static uint16_t bounded16(uint32_t value);
};

static_assert(sizeof(HttpDiagnosticsSnapshot) <= 56,
              "HTTP diagnostics snapshot must remain compact");
