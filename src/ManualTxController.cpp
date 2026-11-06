#include "ManualTxController.h"

bool ManualTxController::turnOff() {
  state_ = ManualTxState::MANUAL_TX_OFF;
  transmitter_.manualTxOff();
  return true;
}

bool ManualTxController::turnOn(bool operationalMode) {
  if (!operationalMode) return false;
  if (state_ == ManualTxState::MANUAL_TX_ON) return true;
  state_ = ManualTxState::MANUAL_TX_ON;
  transmitter_.manualTxOn();
  return true;
}
