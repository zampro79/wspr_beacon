#pragma once

#include <stdint.h>

#include "HttpDiagnostics.h"
#include "WsprScheduler.h"

enum class HttpRfPhase : uint8_t { SAFE, PREPARING, TRANSMITTING };

struct HttpServiceGateInputs {
  bool wifiConnected = false;
  bool rfOperational = false;
  HttpRfPhase rfPhase = HttpRfPhase::SAFE;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  bool nextSlotKnown = false;
  uint32_t millisUntilSlot = 0;
  bool configurationActive = false;
};
class HttpServiceGate {
 public:
  static constexpr uint32_t SLOT_GUARD_MS = 7000UL;

  struct Decision {
    bool allowed = false;
    HttpGateCloseReason reason = HttpGateCloseReason::NONE;
  };

  static Decision evaluate(const HttpServiceGateInputs &inputs);
  static bool allowed(const HttpServiceGateInputs &inputs);
};

struct HttpLifecycleActions {
  bool startListener = false;
  bool stopListener = false;
  bool closeClient = false;
  bool serviceClient = false;
};

class HttpServiceLifecycle {
 public:
  HttpLifecycleActions update(bool serviceAllowed, bool clientActive,
                              uint32_t nowMillis = 0);
  void listenerStarted();
  void listenerStopped() { listenerRunning_ = false; }
  bool listenerRunning() const { return listenerRunning_; }

 private:
  bool listenerRunning_ = false;
};
