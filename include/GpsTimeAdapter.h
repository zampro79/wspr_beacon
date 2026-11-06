#pragma once

#include <stdint.h>

#include "TimeService.h"

struct GpsTimeSample {
  uint32_t utcMillisOfDay = 0;
  uint32_t referenceMonotonicMillis = 0;
  bool sourceAvailable = false;
  bool utcValid = false;
  bool ppsPresent = false;
  bool ppsAssociated = false;
};

class GpsTimeAdapter {
 public:
  bool update(TimeService &service, const GpsTimeSample &sample);

 private:
  uint32_t lastReferenceMonotonicMillis_ = 0;
  TimeQuality lastForwardedQuality_ = TimeQuality::UNKNOWN;
  bool referenceForwarded_ = false;
};
