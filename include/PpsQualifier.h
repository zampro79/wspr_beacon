#pragma once

#include <stdint.h>

class PpsQualifier {
 public:
  static constexpr uint32_t NOMINAL_PERIOD_MS = 1000UL;
  static constexpr uint32_t MIN_PERIOD_MS = 900UL;
  static constexpr uint32_t MAX_PERIOD_MS = 1100UL;
  static constexpr uint8_t REQUIRED_VALID_PERIODS = 3;

  enum class EdgeResult : uint8_t {
    CANDIDATE,
    ACCEPTED,
    QUALIFIED,
    REJECTED
  };

  EdgeResult noteEdge(uint32_t timestampMillis, uint8_t edgeCount = 1);
  bool expire(uint32_t nowMillis);
  void reset();

  bool qualified() const { return qualified_; }
  bool hasCandidate() const { return hasCandidate_; }
  uint8_t validPeriods() const { return validPeriods_; }
  uint32_t lastEdgeMillis() const { return lastEdgeMillis_; }

 private:
  uint32_t lastEdgeMillis_ = 0;
  uint8_t validPeriods_ = 0;
  bool hasCandidate_ = false;
  bool qualified_ = false;

  void startCandidate(uint32_t timestampMillis);
};
