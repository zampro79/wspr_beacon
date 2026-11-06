#include "RfSafetyController.h"

bool RfSafetyController::begin() {
  frequencyPrepared_ = false;
  outputEnabled_ = false;
  if (!driver_.detected()) {
    fault_ = RfFault::NOT_DETECTED;
    return false;
  }
  driver_.disableOutput();
  if (!driver_.initialize(referenceHz_, correction_)) {
    fail(RfFault::INITIALIZATION_FAILED);
    return false;
  }
  fault_ = RfFault::NONE;
  return true;
}

bool RfSafetyController::recover() {
  disableOutput();
  return begin();
}

bool RfSafetyController::configure(uint32_t referenceHz, int32_t correction) {
  if (outputEnabled_) return false;
  referenceHz_ = referenceHz;
  correction_ = correction;
  return true;
}

bool RfSafetyController::prepareFrequency(uint64_t centihertz) {
  frequencyPrepared_ = false;
  if (!operational()) return false;
  if (!driver_.setFrequency(centihertz)) {
    fail(RfFault::FREQUENCY_CONFIGURATION_FAILED);
    return false;
  }
  frequencyPrepared_ = true;
  return true;
}

bool RfSafetyController::enableOutput() {
  if (!operational() || !frequencyPrepared_) return false;
  driver_.enableOutput();
  outputEnabled_ = true;
  return true;
}

void RfSafetyController::disableOutput() {
  driver_.disableOutput();
  outputEnabled_ = false;
}

void RfSafetyController::reportCriticalError() {
  fail(RfFault::CRITICAL_RUNTIME_ERROR);
}

void RfSafetyController::fail(RfFault fault) {
  driver_.disableOutput();
  outputEnabled_ = false;
  frequencyPrepared_ = false;
  fault_ = fault;
}
