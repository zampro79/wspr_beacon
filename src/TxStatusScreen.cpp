#include "TxStatusScreen.h"

#include <stdio.h>
#include <string.h>

namespace {
uint8_t centeredX(const char *text, uint8_t characterWidth) {
  const uint16_t width = strlen(text) * characterWidth;
  return width < 128 ? static_cast<uint8_t>((128U - width) / 2U) : 0;
}
}  // namespace

TxStatusLayout TxStatusScreen::build(const TxPreparedSnapshot &prepared) {
  TxStatusLayout layout;
  snprintf(layout.message, sizeof(layout.message), "%s %s %u",
           prepared.message.callsign, prepared.message.locator,
           prepared.message.powerDbm);
  const uint32_t mhz = prepared.frequencyHz / 1000000UL;
  const uint32_t fraction = prepared.frequencyHz % 1000000UL;
  snprintf(layout.frequency, sizeof(layout.frequency), "%lu.%06lu MHz",
           static_cast<unsigned long>(mhz),
           static_cast<unsigned long>(fraction));
  layout.messageX = centeredX(layout.message, 6);
  layout.frequencyX = centeredX(layout.frequency, 6);
  return layout;
}
