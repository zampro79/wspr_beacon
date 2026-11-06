#pragma once

#include <stdint.h>

#include "BuildProfile.h"

enum class LocatorSource : uint8_t {
  FIXED,
  GPS_LIVE,
  GPS_LAST_KNOWN
};

struct LocatorSnapshot {
  char locator[5] = WSPR_DEFAULT_LOCATOR_LITERAL;
  LocatorSource source = LocatorSource::FIXED;
};

class LocatorService {
 public:
  void update(const char *fixedLocator, bool gpsFixValid,
              const char *gpsLocator);
  LocatorSnapshot snapshot() const { return snapshot_; }

  static bool validMaidenhead4(const char *locator);

 private:
  LocatorSnapshot snapshot_;
  char lastGpsLocator_[5] = "----";
  bool hasLastGpsLocator_ = false;

  static void copyLocator(char destination[5], const char *source);
};

static_assert(sizeof(LocatorSnapshot) <= 6,
              "LocatorSnapshot must remain compact");
