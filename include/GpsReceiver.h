#pragma once

#include <Arduino.h>

#include "DiagnosticRegistry.h"
#include "NmeaParser.h"
#include "PpsQualifier.h"

struct GpsData {
  bool nmeaAvailable = false;
  bool timeValid = false;
  bool positionValid = false;
  bool ppsAvailable = false;
  bool ppsAssociated = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t satellites = 0;
  float latitude = 0;
  float longitude = 0;
  float altitude = 0;
  char locator[5] = "----";
  uint32_t lastSentenceMillis = 0;
};

class GpsReceiver {
 public:
  explicit GpsReceiver(Stream &stream) : stream_(stream) {}
  void update();
  void notePps(uint32_t timestampMillis, uint8_t edgeCount = 1);
  const GpsData &data() const { return data_; }
  uint32_t utcMillisOfDay() const;
  uint32_t utcMillisOfDayAt(uint32_t monotonicMillis) const;
  bool synchronized(uint32_t maxAgeMs = 5000) const;

 private:
  Stream &stream_;
  GpsData data_;
  NmeaLineBuffer lineBuffer_;
  uint32_t syncMillis_ = 0;
  uint32_t syncUtcSeconds_ = 0;
  uint32_t lastPpsMillis_ = 0;
  uint32_t lastAssociatedPpsMillis_ = 0;
  uint32_t lastAssociatedUtcSeconds_ = 0;
  PpsQualifier ppsQualifier_;
  bool pendingPps_ = false;
  bool haveAssociatedPps_ = false;
  bool gpsTimedOut_ = false;

  void consume(char value);
  void parseSentence(char *sentence);
  void updateTime(uint8_t hour, uint8_t minute, uint8_t second);
  void revokePpsAssociation(uint32_t timestampMillis,
                            DiagnosticReason reason);
  bool associationCoherent(uint32_t utcSeconds,
                           uint32_t ppsMillis) const;
};
