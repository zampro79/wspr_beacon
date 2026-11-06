#include "PpsQualifier.h"

void PpsQualifier::startCandidate(uint32_t timestampMillis) {
  lastEdgeMillis_ = timestampMillis;
  validPeriods_ = 0;
  hasCandidate_ = true;
  qualified_ = false;
}

PpsQualifier::EdgeResult PpsQualifier::noteEdge(uint32_t timestampMillis,
                                                uint8_t edgeCount) {
  if (edgeCount != 1) {
    reset();
    if (edgeCount > 0) startCandidate(timestampMillis);
    return EdgeResult::REJECTED;
  }
  if (!hasCandidate_) {
    startCandidate(timestampMillis);
    return EdgeResult::CANDIDATE;
  }

  const uint32_t periodMillis = timestampMillis - lastEdgeMillis_;
  if (periodMillis < MIN_PERIOD_MS || periodMillis > MAX_PERIOD_MS) {
    startCandidate(timestampMillis);
    return EdgeResult::REJECTED;
  }

  lastEdgeMillis_ = timestampMillis;
  if (validPeriods_ < REQUIRED_VALID_PERIODS) ++validPeriods_;
  if (validPeriods_ >= REQUIRED_VALID_PERIODS) {
    qualified_ = true;
    return EdgeResult::QUALIFIED;
  }
  return EdgeResult::ACCEPTED;
}

bool PpsQualifier::expire(uint32_t nowMillis) {
  if (!hasCandidate_ ||
      static_cast<uint32_t>(nowMillis - lastEdgeMillis_) <= MAX_PERIOD_MS) {
    return false;
  }
  const bool wasQualified = qualified_;
  reset();
  return wasQualified;
}

void PpsQualifier::reset() {
  lastEdgeMillis_ = 0;
  validPeriods_ = 0;
  hasCandidate_ = false;
  qualified_ = false;
}
