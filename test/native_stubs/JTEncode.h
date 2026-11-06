#pragma once

#include <stdint.h>
#include <string.h>

#define WSPR_SYMBOL_COUNT 162

class JTEncode {
 public:
  char lastCallsign[7] = {};
  char lastLocator[5] = {};
  uint8_t lastPower = 0;
  uint8_t firstSymbol = 0;
  uint32_t encodeCalls = 0;

  void wspr_encode(const char *callsign, const char *locator, uint8_t power,
                   uint8_t *symbols) {
    strncpy(lastCallsign, callsign, sizeof(lastCallsign) - 1U);
    strncpy(lastLocator, locator, sizeof(lastLocator) - 1U);
    lastPower = power;
    ++encodeCalls;
    for (uint16_t index = 0; index < WSPR_SYMBOL_COUNT; ++index) {
      symbols[index] = static_cast<uint8_t>((index + firstSymbol) % 4U);
    }
  }
};
