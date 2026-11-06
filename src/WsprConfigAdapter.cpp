#include "WsprConfigAdapter.h"

#include <string.h>

#include "ConfigValidator.h"

bool WsprConfigAdapter::apply(const BeaconSettings &settings,
                              bool transmitterBusy, BeaconConfig &target) {
  if (transmitterBusy ||
      ConfigValidator::validate(settings) != ConfigValidation::VALID) {
    return false;
  }
  strcpy(target.callsign, settings.callsign);
  strcpy(target.fixedLocator, settings.fixedLocator);
  target.powerDbm = settings.powerDbm;
  target.transmitIntervalMinutes = settings.transmitIntervalMinutes;
  target.baseFrequencyHz = settings.baseFrequencyHz;
  target.frequencyOffsetHz = settings.frequencyOffsetHz;
  target.si5351Correction = settings.si5351Correction;
  target.band = settings.band;
  target.frequencyMode = settings.frequencyMode;
  return true;
}
