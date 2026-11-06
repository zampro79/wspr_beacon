#include "TimeService.h"

namespace {
constexpr uint32_t UTC_MILLIS_PER_DAY = 86400000UL;
constexpr uint16_t UTC_VALID_FLAG = 1U << 0;
constexpr uint16_t GPS_AVAILABLE_FLAG = 1U << 1;
constexpr uint16_t NTP_AVAILABLE_FLAG = 1U << 2;
constexpr uint16_t GPS_VALID_FLAG = 1U << 3;
constexpr uint16_t NTP_VALID_FLAG = 1U << 4;

uint16_t availableFlag(TimeSource source) {
  if (source == TimeSource::GPS) return GPS_AVAILABLE_FLAG;
  if (source == TimeSource::NTP) return NTP_AVAILABLE_FLAG;
  return 0;
}
uint16_t validFlag(TimeSource source) {
  if (source == TimeSource::GPS) return GPS_VALID_FLAG;
  if (source == TimeSource::NTP) return NTP_VALID_FLAG;
  return 0;
}
}  // namespace

bool TimeSnapshot::utcValid() const {
  return (statusFlags & UTC_VALID_FLAG) != 0;
}

bool TimeSnapshot::sourceAvailable(TimeSource candidate) const {
  return (statusFlags & availableFlag(candidate)) != 0;
}

bool TimeSnapshot::sourceValid(TimeSource candidate) const {
  return (statusFlags & validFlag(candidate)) != 0;
}

TimeService::TimeService(const TimeServiceConfig &config) : config_(config) {}

TimeService::SourceState *TimeService::state(TimeSource source) {
  if (source == TimeSource::GPS) return &gps_;
  if (source == TimeSource::NTP) return &ntp_;
  return nullptr;
}

const TimeService::SourceState *TimeService::state(TimeSource source) const {
  if (source == TimeSource::GPS) return &gps_;
  if (source == TimeSource::NTP) return &ntp_;
  return nullptr;
}

uint32_t TimeService::timeout(TimeSource source) const {
  if (source == TimeSource::GPS) return config_.gpsTimeoutMillis;
  if (source == TimeSource::NTP) return config_.ntpTimeoutMillis;
  return 0;
}

bool TimeService::setSourceAvailable(TimeSource source, bool available) {
  SourceState *sourceState = state(source);
  if (sourceState == nullptr) return false;
  sourceState->available = available;
  if (!available) sourceState->valid = false;
  return true;
}

bool TimeService::update(TimeSource source, const TimeReference &reference,
                         uint32_t monotonicMillis) {
  SourceState *sourceState = state(source);
  if (sourceState == nullptr || !reference.utcValid ||
      reference.utcMillisOfDay >= UTC_MILLIS_PER_DAY) {
    return false;
  }

  sourceState->utcMillisOfDay = reference.utcMillisOfDay;
  sourceState->referenceMonotonicMillis = monotonicMillis;
  sourceState->quality = reference.quality;
  sourceState->available = true;
  sourceState->valid = true;
  return true;
}

bool TimeService::invalidate(TimeSource source) {
  SourceState *sourceState = state(source);
  if (sourceState == nullptr) return false;
  sourceState->valid = false;
  return true;
}

bool TimeService::fresh(TimeSource source, uint32_t monotonicMillis) const {
  const SourceState *sourceState = state(source);
  if (sourceState == nullptr || !sourceState->available || !sourceState->valid) {
    return false;
  }
  return static_cast<uint32_t>(monotonicMillis -
                               sourceState->referenceMonotonicMillis) <=
         timeout(source);
}

TimeSnapshot TimeService::snapshot(uint32_t monotonicMillis) const {
  TimeSnapshot result;

  if (gps_.available) result.statusFlags |= GPS_AVAILABLE_FLAG;
  if (ntp_.available) result.statusFlags |= NTP_AVAILABLE_FLAG;
  if (fresh(TimeSource::GPS, monotonicMillis)) {
    result.statusFlags |= GPS_VALID_FLAG;
  }
  if (fresh(TimeSource::NTP, monotonicMillis)) {
    result.statusFlags |= NTP_VALID_FLAG;
  }

  if (result.sourceValid(TimeSource::GPS) &&
      gps_.quality == TimeQuality::PRECISE) {
    result.source = TimeSource::GPS;
  } else if (result.sourceValid(TimeSource::NTP)) {
    result.source = TimeSource::NTP;
  } else if (result.sourceValid(TimeSource::GPS)) {
    result.source = TimeSource::GPS;
  } else {
    return result;
  }

  const SourceState *selected = state(result.source);
  result.referenceAgeMillis = static_cast<uint32_t>(
      monotonicMillis - selected->referenceMonotonicMillis);
  result.utcMillisOfDay =
      (selected->utcMillisOfDay +
       (result.referenceAgeMillis % UTC_MILLIS_PER_DAY)) %
      UTC_MILLIS_PER_DAY;
  result.quality = selected->quality;
  result.statusFlags |= UTC_VALID_FLAG;
  return result;
}
