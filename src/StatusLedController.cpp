#include "StatusLedController.h"

void StatusLedController::begin(uint32_t monotonicMillis) {
  modeStartedMillis_ = monotonicMillis;
  initialized_ = true;
  mode_ = StatusLedMode::OFF;
  outputEnabled_ = false;
}

StatusLedOutput StatusLedController::update(
    uint32_t monotonicMillis, const StatusLedInputs &inputs) {
  if (!initialized_) begin(monotonicMillis);
  const StatusLedMode selected = selectMode(monotonicMillis, inputs);
  if (selected != mode_) {
    mode_ = selected;
    modeStartedMillis_ = monotonicMillis;
  }
  const bool enabled = pattern(
      mode_, static_cast<uint32_t>(monotonicMillis - modeStartedMillis_));
  StatusLedOutput output;
  output.enabled = enabled;
  output.changed = enabled != outputEnabled_;
  output.mode = mode_;
  outputEnabled_ = enabled;
  return output;
}

StatusLedMode StatusLedController::selectMode(
    uint32_t, const StatusLedInputs &inputs) const {
  if (inputs.startupActive) return StatusLedMode::STARTUP;
  if (inputs.hardwareTestActive) return StatusLedMode::HARDWARE_TEST;
  if (inputs.rfError) return StatusLedMode::RF_ERROR;
  if (inputs.transmitting) return StatusLedMode::TRANSMITTING;
  if (inputs.preparing) return StatusLedMode::PREPARING;
  if (inputs.wifiConfiguring) return StatusLedMode::WIFI_CONFIG;
  if (inputs.ready) return StatusLedMode::READY;
  return StatusLedMode::WAITING_SYNC;
}

bool StatusLedController::pattern(StatusLedMode mode, uint32_t elapsedMillis) {
  switch (mode) {
    case StatusLedMode::OFF:
      return false;
    case StatusLedMode::STARTUP:
      return true;
    case StatusLedMode::HARDWARE_TEST:
      return elapsedMillis < 600UL && (elapsedMillis % 200UL) < 100UL;
    case StatusLedMode::WAITING_SYNC:
      return (elapsedMillis % 2000UL) < 100UL;
    case StatusLedMode::READY:
      return (elapsedMillis % 3000UL) < 100UL;
    case StatusLedMode::WIFI_CONFIG: {
      const uint32_t phase = elapsedMillis % 2000UL;
      return phase < 100UL || (phase >= 200UL && phase < 300UL);
    }
    case StatusLedMode::PREPARING:
      return (elapsedMillis % 300UL) < 150UL;
    case StatusLedMode::TRANSMITTING:
      return true;
    case StatusLedMode::RF_ERROR: {
      const uint32_t phase = elapsedMillis % 2000UL;
      return phase < 100UL || (phase >= 200UL && phase < 300UL) ||
             (phase >= 400UL && phase < 500UL);
    }
  }
  return false;
}
