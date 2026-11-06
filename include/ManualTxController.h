#pragma once

#include <stdint.h>

#include "WsprTransmitter.h"

enum class ManualTxState : uint8_t { MANUAL_TX_OFF, MANUAL_TX_ON };

class ManualTxController {
 public:
  explicit ManualTxController(WsprTransmitter &transmitter)
      : transmitter_(transmitter) {}

  bool turnOff();
  bool turnOn(bool operationalMode);
  ManualTxState state() const { return state_; }
  bool permitted() const { return state_ == ManualTxState::MANUAL_TX_ON; }

 private:
  WsprTransmitter &transmitter_;
  ManualTxState state_ = ManualTxState::MANUAL_TX_ON;
};
