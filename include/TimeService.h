#pragma once

#include <stdint.h>

enum class TimeSource : uint8_t { NONE, GPS, NTP };

enum class TimeQuality : uint8_t {
  UNKNOWN,
  COARSE,
  SYNCHRONIZED,
  PRECISE
};

struct TimeReference {
  uint32_t utcMillisOfDay = 0;
  TimeQuality quality = TimeQuality::UNKNOWN;
  bool utcValid = false;
};

struct TimeServiceConfig {
  constexpr TimeServiceConfig(uint32_t gpsTimeout = 0,
                              uint32_t ntpTimeout = 0)
      : gpsTimeoutMillis(gpsTimeout), ntpTimeoutMillis(ntpTimeout) {}

  uint32_t gpsTimeoutMillis;
  uint32_t ntpTimeoutMillis;
};

struct TimeSnapshot {
  uint32_t utcMillisOfDay = 0;
  uint32_t referenceAgeMillis = 0;
  uint16_t statusFlags = 0;
  TimeSource source = TimeSource::NONE;
  TimeQuality quality = TimeQuality::UNKNOWN;

  bool utcValid() const;
  bool sourceAvailable(TimeSource candidate) const;
  bool sourceValid(TimeSource candidate) const;
};

class TimeService {
 public:
  explicit TimeService(const TimeServiceConfig &config);

  bool setSourceAvailable(TimeSource source, bool available);
  bool update(TimeSource source, const TimeReference &reference,
              uint32_t monotonicMillis);
  bool invalidate(TimeSource source);
  TimeSnapshot snapshot(uint32_t monotonicMillis) const;

 private:
  struct SourceState {
    uint32_t utcMillisOfDay = 0;
    uint32_t referenceMonotonicMillis = 0;
    TimeQuality quality = TimeQuality::UNKNOWN;
    bool available = false;
    bool valid = false;
  };

  TimeServiceConfig config_;
  SourceState gps_;
  SourceState ntp_;

  SourceState *state(TimeSource source);
  const SourceState *state(TimeSource source) const;
  uint32_t timeout(TimeSource source) const;
  bool fresh(TimeSource source, uint32_t monotonicMillis) const;
};

static_assert(sizeof(TimeSnapshot) == 12, "TimeSnapshot must remain compact");
