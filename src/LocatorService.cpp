#include "LocatorService.h"

#include <string.h>

bool LocatorService::validMaidenhead4(const char *locator) {
  return locator != nullptr && strlen(locator) == 4 &&
         locator[0] >= 'A' && locator[0] <= 'R' &&
         locator[1] >= 'A' && locator[1] <= 'R' &&
         locator[2] >= '0' && locator[2] <= '9' &&
         locator[3] >= '0' && locator[3] <= '9';
}

void LocatorService::copyLocator(char destination[5], const char *source) {
  memcpy(destination, source, 4);
  destination[4] = '\0';
}

void LocatorService::update(const char *fixedLocator, bool gpsFixValid,
                            const char *gpsLocator) {
  if (gpsFixValid && validMaidenhead4(gpsLocator)) {
    copyLocator(lastGpsLocator_, gpsLocator);
    copyLocator(snapshot_.locator, gpsLocator);
    hasLastGpsLocator_ = true;
    snapshot_.source = LocatorSource::GPS_LIVE;
    return;
  }

  if (hasLastGpsLocator_) {
    copyLocator(snapshot_.locator, lastGpsLocator_);
    snapshot_.source = LocatorSource::GPS_LAST_KNOWN;
    return;
  }

  const char *fallback = validMaidenhead4(fixedLocator)
                             ? fixedLocator
                             : WSPR_DEFAULT_LOCATOR;
  copyLocator(snapshot_.locator, fallback);
  snapshot_.source = LocatorSource::FIXED;
}
