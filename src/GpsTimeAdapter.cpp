#include "GpsTimeAdapter.h"

bool GpsTimeAdapter::update(TimeService &service, const GpsTimeSample &sample) {
  service.setSourceAvailable(TimeSource::GPS, sample.sourceAvailable);
  if (!sample.sourceAvailable || !sample.utcValid) {
    service.invalidate(TimeSource::GPS);
    referenceForwarded_ = false;
    return false;
  }
  const TimeQuality quality = sample.ppsPresent && sample.ppsAssociated
                                  ? TimeQuality::PRECISE
                                  : TimeQuality::COARSE;
  if (referenceForwarded_ &&
      sample.referenceMonotonicMillis == lastReferenceMonotonicMillis_ &&
      quality == lastForwardedQuality_) {
    return false;
  }

  TimeReference reference;
  reference.utcMillisOfDay = sample.utcMillisOfDay;
  reference.quality = quality;
  reference.utcValid = true;
  if (!service.update(TimeSource::GPS, reference,
                      sample.referenceMonotonicMillis)) {
    return false;
  }
  lastReferenceMonotonicMillis_ = sample.referenceMonotonicMillis;
  lastForwardedQuality_ = quality;
  referenceForwarded_ = true;
  return true;
}
