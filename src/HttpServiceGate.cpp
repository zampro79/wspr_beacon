#include "HttpServiceGate.h"

HttpServiceGate::Decision HttpServiceGate::evaluate(
    const HttpServiceGateInputs &inputs) {
  Decision decision;
  if (!inputs.wifiConnected) {
    decision.reason = HttpGateCloseReason::WIFI_DISCONNECTED;
  } else if (inputs.rfPhase != HttpRfPhase::SAFE) {
    decision.reason = HttpGateCloseReason::RF_CRITICAL;
  } else if (inputs.configurationActive) {
    decision.allowed = true;
  } else if (!inputs.rfOperational ||
             inputs.authorization == WsprTimeAuthorization::NONE) {
    decision.allowed = true;
  } else if (!inputs.nextSlotKnown) {
    decision.reason = HttpGateCloseReason::TIME_UNCERTAIN;
  } else if (inputs.millisUntilSlot <= SLOT_GUARD_MS) {
    decision.reason = HttpGateCloseReason::SLOT_GUARD;
  } else {
    decision.allowed = true;
  }
  return decision;
}

bool HttpServiceGate::allowed(const HttpServiceGateInputs &inputs) {
  return evaluate(inputs).allowed;
}

HttpLifecycleActions HttpServiceLifecycle::update(bool serviceAllowed,
                                                  bool clientActive,
                                                  uint32_t nowMillis) {
  HttpLifecycleActions result;
  if (!serviceAllowed) {
    result.closeClient = clientActive;
    result.stopListener = listenerRunning_;
    listenerRunning_ = false;
    return result;
  }
  if (!listenerRunning_) {
    result.startListener = true;
  }
  result.serviceClient = listenerRunning_;
  (void)nowMillis;
  return result;
}

void HttpServiceLifecycle::listenerStarted() {
  listenerRunning_ = true;
}
