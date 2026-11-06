#pragma once

#include <stdint.h>

#include "TxPreparedSnapshot.h"

struct TxStatusLayout {
  static constexpr uint8_t TITLE_X = 13;
  static constexpr uint8_t TITLE_Y = 0;
  static constexpr uint8_t TX_X = 34;
  static constexpr uint8_t TX_Y = 17;
  static constexpr uint8_t MESSAGE_Y = 42;
  static constexpr uint8_t FREQUENCY_Y = 54;
  char message[17] = {};
  char frequency[22] = {};
  uint8_t messageX = 0;
  uint8_t frequencyX = 0;
};
class TxStatusScreen {
 public:
  static TxStatusLayout build(const TxPreparedSnapshot &prepared);
};
