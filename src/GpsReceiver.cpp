#include "GpsReceiver.h"

#include "DiagnosticRegistry.h"
#include "Maidenhead.h"

#if !defined(ARDUINO)
unsigned long millis() __attribute__((weak));
unsigned long millis() { return 0; }
#endif

void GpsReceiver::update() {
  const uint32_t nowMillis = millis();
  if (ppsQualifier_.expire(nowMillis)) {
    revokePpsAssociation(nowMillis, DiagnosticReason::TIMEOUT);
  }
  while (stream_.available() > 0) consume(static_cast<char>(stream_.read()));
  if (data_.ppsAvailable &&
      static_cast<uint32_t>(millis() - lastPpsMillis_) > 2500UL) {
    data_.ppsAvailable = false;
    diagnostics.setState(DiagnosticModule::PPS, DiagnosticState::STATE_DEGRADED,
                         DiagnosticReason::TIMEOUT, millis());
  }
  if (data_.timeValid && !synchronized() && !gpsTimedOut_) {
    gpsTimedOut_ = true;
    diagnostics.setState(DiagnosticModule::GPS, DiagnosticState::STATE_DEGRADED,
                         DiagnosticReason::TIMEOUT, millis());
  }
}

void GpsReceiver::notePps(uint32_t timestampMillis, uint8_t edgeCount) {
  if (edgeCount == 0) return;
  lastPpsMillis_ = timestampMillis;
  data_.ppsAvailable = true;
  const PpsQualifier::EdgeResult result =
      ppsQualifier_.noteEdge(timestampMillis, edgeCount);
  if (result == PpsQualifier::EdgeResult::REJECTED) {
    diagnostics.recordFailure(DiagnosticModule::PPS);
    revokePpsAssociation(timestampMillis, DiagnosticReason::INVALID_DATA);
    return;
  }
  if (!ppsQualifier_.qualified()) {
    pendingPps_ = false;
    data_.ppsAssociated = false;
    diagnostics.setState(DiagnosticModule::PPS,
                         DiagnosticState::STATE_INITIALIZING,
                         DiagnosticReason::WAITING_FOR_DATA, timestampMillis);
    return;
  }
  pendingPps_ = true;
  diagnostics.recordSuccess(DiagnosticModule::PPS);
  diagnostics.setState(DiagnosticModule::PPS, DiagnosticState::STATE_OPERATIONAL,
                       DiagnosticReason::NONE, timestampMillis);
}

void GpsReceiver::consume(char value) {
  if (lineBuffer_.push(value)) {
    parseSentence(lineBuffer_.sentence());
    lineBuffer_.reset();
  }
}

void GpsReceiver::parseSentence(char *sentence) {
  NmeaUpdate update;
  if (!NmeaParser::parse(sentence, update)) return;
  data_.nmeaAvailable = true;
  gpsTimedOut_ = false;
  diagnostics.recordSuccess(DiagnosticModule::GPS);
  diagnostics.setState(DiagnosticModule::GPS, DiagnosticState::STATE_OPERATIONAL,
                       DiagnosticReason::NONE, millis());
  if (update.timeValid) updateTime(update.hour, update.minute, update.second);
  if (update.type == NmeaSentenceType::GGA) {
    data_.satellites = update.satellites;
    data_.altitude = update.altitude;
  }
  data_.positionValid = update.positionValid;
  if (update.positionValid) {
    data_.latitude = update.latitude;
    data_.longitude = update.longitude;
    maidenhead4(update.latitude, update.longitude, data_.locator);
  }
}

void GpsReceiver::updateTime(uint8_t hour, uint8_t minute, uint8_t second) {
  data_.hour = hour;
  data_.minute = minute;
  data_.second = second;
  data_.timeValid = true;
  syncUtcSeconds_ = static_cast<uint32_t>(hour) * 3600UL + minute * 60UL + data_.second;
  const uint32_t sentenceMillis = millis();
  // NEO-6 sentences normally describe the epoch marked by the latest PPS.
  // Only a qualified periodic signal may anchor UTC; otherwise retain the
  // NMEA-only fallback as coarse time.
  const uint32_t qualifiedPpsMillis = ppsQualifier_.lastEdgeMillis();
  const bool ppsRecent = ppsQualifier_.qualified() &&
                         static_cast<uint32_t>(sentenceMillis -
                                               qualifiedPpsMillis) <= 1200UL;
  if (pendingPps_ && ppsRecent) {
    if (associationCoherent(syncUtcSeconds_, qualifiedPpsMillis)) {
      syncMillis_ = qualifiedPpsMillis;
      lastAssociatedPpsMillis_ = qualifiedPpsMillis;
      lastAssociatedUtcSeconds_ = syncUtcSeconds_;
      haveAssociatedPps_ = true;
      pendingPps_ = false;
      data_.ppsAssociated = true;
    } else {
      ppsQualifier_.reset();
      revokePpsAssociation(sentenceMillis, DiagnosticReason::INVALID_DATA);
      syncMillis_ = sentenceMillis;
    }
  } else if (ppsRecent && haveAssociatedPps_ &&
             qualifiedPpsMillis == lastAssociatedPpsMillis_ &&
             syncUtcSeconds_ == lastAssociatedUtcSeconds_) {
    // GGA and RMC can report the same epoch for the same physical edge.
    syncMillis_ = lastAssociatedPpsMillis_;
    data_.ppsAssociated = true;
  } else {
    syncMillis_ = sentenceMillis;
    data_.ppsAssociated = false;
    if (pendingPps_ && !ppsRecent) pendingPps_ = false;
  }
  data_.lastSentenceMillis = sentenceMillis;
}

void GpsReceiver::revokePpsAssociation(uint32_t timestampMillis,
                                       DiagnosticReason reason) {
  pendingPps_ = false;
  data_.ppsAssociated = false;
  haveAssociatedPps_ = false;
  diagnostics.setState(DiagnosticModule::PPS, DiagnosticState::STATE_DEGRADED,
                       reason, timestampMillis);
}

bool GpsReceiver::associationCoherent(uint32_t utcSeconds,
                                      uint32_t ppsMillis) const {
  if (!haveAssociatedPps_) return true;
  if (ppsMillis == lastAssociatedPpsMillis_) {
    return utcSeconds == lastAssociatedUtcSeconds_;
  }
  const uint32_t elapsedMillis = ppsMillis - lastAssociatedPpsMillis_;
  const uint32_t elapsedSeconds = (elapsedMillis + 500UL) / 1000UL;
  const uint32_t utcElapsed =
      (utcSeconds + 86400UL - lastAssociatedUtcSeconds_) % 86400UL;
  return elapsedSeconds > 0 && utcElapsed == elapsedSeconds;
}

uint32_t GpsReceiver::utcMillisOfDay() const {
  if (!data_.timeValid) return 0;
  return utcMillisOfDayAt(millis());
}

uint32_t GpsReceiver::utcMillisOfDayAt(uint32_t monotonicMillis) const {
  if (!data_.timeValid) return 0;
  return (syncUtcSeconds_ * 1000UL + monotonicMillis - syncMillis_) % 86400000UL;
}

bool GpsReceiver::synchronized(uint32_t maxAgeMs) const {
  return data_.timeValid && static_cast<uint32_t>(millis() - data_.lastSentenceMillis) <= maxAgeMs;
}
