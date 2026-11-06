#include "IdleStatusScreen.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t UTC_MILLIS_PER_DAY = 86400000UL;

const char *rfLabel(OledRfState state) {
  switch (state) {
    case OledRfState::READY: return "READY";
    case OledRfState::WAITING_TIME: return "WAIT TIME";
    case OledRfState::INHIBITED: return "INHIBITED";
    case OledRfState::ERROR: return "ERROR";
  }
  return "INHIBITED";
}
}  // namespace

IdleStatusSnapshot IdleStatusScreen::build(const IdleStatusInputs &inputs) {
  IdleStatusSnapshot result;
  if (inputs.callsign == nullptr || inputs.callsign[0] == '\0') {
    strcpy(result.lines[0], "NO CALLSIGN");
  } else {
    snprintf(result.lines[0], sizeof(result.lines[0]), "MSG: %.6s %.4s %u",
             inputs.callsign,
             inputs.locator == nullptr ? "----" : inputs.locator,
             inputs.powerDbm);
  }

  char *frequencyLine = result.lines[1];
  if (inputs.frequencyMode == WsprFrequencyMode::RANDOM) {
    strcpy(frequencyLine, "FREQ: RANDOM ");
  } else {
    const uint32_t mhz = inputs.frequencyHz / 1000000UL;
    const uint32_t hzFraction = inputs.frequencyHz % 1000000UL;
    strcpy(frequencyLine, "FREQ: ");
    frequencyLine[6] = mhz <= 99 ? static_cast<char>('0' + mhz / 10UL) : '-';
    frequencyLine[7] = mhz <= 99 ? static_cast<char>('0' + mhz % 10UL) : '-';
    frequencyLine[8] = '.';
    uint32_t divisor = 100000UL;
    for (uint8_t index = 9; index < 15; ++index) {
      frequencyLine[index] =
          static_cast<char>('0' + (hzFraction / divisor) % 10UL);
      divisor /= 10UL;
    }
    frequencyLine[15] = ' ';
  }
  const uint8_t bandStart = strlen(frequencyLine);
  frequencyLine[bandStart] = '(';
  const char *band = inputs.band == nullptr ? "---" : inputs.band;
  bool bandEnded = false;
  for (uint8_t index = 0; index < 3; ++index) {
    if (!bandEnded && band[index] == '\0') bandEnded = true;
    frequencyLine[bandStart + 1U + index] = bandEnded ? '-' : band[index];
  }
  frequencyLine[bandStart + 4U] = ')';
  frequencyLine[bandStart + 5U] = '\0';
  snprintf(result.lines[2], sizeof(result.lines[2]), "RF: %s",
           rfLabel(inputs.rfState));

  if (!inputs.nextSlotValid || !inputs.time.utcValid()) {
    strcpy(result.lines[3], "NEXT TX: --:-- UTC");
    strcpy(result.lines[4], "REMAINING: --");
    return result;
  }

  const uint32_t slotSeconds = inputs.nextSlotUtcMillis / 1000UL;
  const uint8_t hour = slotSeconds / 3600UL;
  const uint8_t minute = (slotSeconds / 60UL) % 60UL;
  snprintf(result.lines[3], sizeof(result.lines[3]),
           "NEXT TX: %02u:%02u UTC", hour, minute);

  const uint32_t remainingMillis =
      (inputs.nextSlotUtcMillis + UTC_MILLIS_PER_DAY -
       inputs.time.utcMillisOfDay) % UTC_MILLIS_PER_DAY;
  const uint32_t remainingSeconds = (remainingMillis + 999UL) / 1000UL;
  snprintf(result.lines[4], sizeof(result.lines[4]), "REMAINING: %lus",
           static_cast<unsigned long>(remainingSeconds));
  return result;
}
