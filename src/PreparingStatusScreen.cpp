#include "PreparingStatusScreen.h"

#include <stdio.h>
#include <string.h>

namespace {
uint8_t centeredX(const char *text) {
  const uint16_t width = strlen(text) * 6U;
  return width < 128 ? static_cast<uint8_t>((128U - width) / 2U) : 0;
}
}  // namespace

PreparingStatusLayout PreparingStatusScreen::build(
    const TxPreparedSnapshot &prepared) {
  PreparingStatusLayout layout;
  snprintf(layout.message, sizeof(layout.message), "%s %s %u",
           prepared.message.callsign, prepared.message.locator,
           prepared.message.powerDbm);
  const uint32_t mhz = prepared.frequencyHz / 1000000UL;
  const uint32_t fraction = prepared.frequencyHz % 1000000UL;
  snprintf(layout.frequency, sizeof(layout.frequency), "%lu.%06lu MHz",
           static_cast<unsigned long>(mhz),
           static_cast<unsigned long>(fraction));
  layout.messageX = centeredX(layout.message);
  layout.frequencyX = centeredX(layout.frequency);
  return layout;
}
