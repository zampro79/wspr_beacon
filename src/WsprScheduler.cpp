#include "WsprScheduler.h"

WsprTimeAuthorization WsprScheduler::authorizationFor(
    const TimeSnapshot &time) {
  if (!time.utcValid()) return WsprTimeAuthorization::NONE;
  if (time.source == TimeSource::GPS &&
      time.quality == TimeQuality::PRECISE) {
    return WsprTimeAuthorization::GPS_PRECISE;
  }
  if (time.source == TimeSource::NTP &&
      time.quality == TimeQuality::SYNCHRONIZED) {
    return WsprTimeAuthorization::NTP_EXPERIMENTAL;
  }
  return WsprTimeAuthorization::NONE;
}

bool WsprScheduler::nextSlotUtcMillis(const TimeSnapshot &time,
                                      uint8_t intervalMinutes,
                                      uint32_t &nextSlotUtcMillis) {
  if (!time.utcValid() || intervalMinutes == 0) return false;
  const uint32_t cycleMillis =
      static_cast<uint32_t>(intervalMinutes) * 60000UL;
  nextSlotUtcMillis =
      ((time.utcMillisOfDay / cycleMillis + 1UL) * cycleMillis) %
      86400000UL;
  return true;
}

bool WsprScheduler::millisUntilNextSlot(const TimeSnapshot &time,
                                        uint8_t intervalMinutes,
                                        uint32_t &remainingMillis) {
  if (!time.utcValid() || intervalMinutes == 0) return false;
  const uint32_t cycleMillis =
      static_cast<uint32_t>(intervalMinutes) * 60000UL;
  const uint32_t phaseMillis = time.utcMillisOfDay % cycleMillis;
  remainingMillis = cycleMillis - phaseMillis;
  return true;
}

bool WsprScheduler::arm(const TimeSnapshot &time, uint32_t nowMillis,
                        uint32_t nowMicros,
                        uint8_t intervalMinutes, uint8_t preparationSeconds) {
  armed_ = false;
  authorization_ = WsprTimeAuthorization::NONE;
  const WsprTimeAuthorization authorization = authorizationFor(time);
  if (authorization == WsprTimeAuthorization::NONE || intervalMinutes == 0) {
    return false;
  }
  const uint32_t cycleMillis =
      static_cast<uint32_t>(intervalMinutes) * 60000UL;
  const uint32_t preparationMillis =
      static_cast<uint32_t>(preparationSeconds) * 1000UL;
  const uint32_t phaseMillis = time.utcMillisOfDay % cycleMillis;
  const uint32_t untilSlotMillis = cycleMillis - phaseMillis;
  if (untilSlotMillis > preparationMillis) return false;

  slotStartMillis_ = nowMillis + untilSlotMillis;
  slotStartMicros_ = nowMicros + untilSlotMillis * 1000UL;
  slot_ = (time.utcMillisOfDay / cycleMillis) + 1UL;
  authorization_ = authorization;
  armed_ = true;
  return true;
}

WsprStartDecision WsprScheduler::startDecision(uint32_t nowMillis) const {
  if (!armed_) return WsprStartDecision::WAIT;
  const int32_t delta = static_cast<int32_t>(nowMillis - slotStartMillis_);
  if (delta < 0) return WsprStartDecision::WAIT;
  if (static_cast<uint32_t>(delta) >= START_WINDOW_MS) {
    return WsprStartDecision::MISSED;
  }
  return WsprStartDecision::START;
}

bool WsprScheduler::millisUntilSlot(uint32_t nowMillis,
                                   uint32_t &remainingMillis) const {
  if (!armed_) return false;
  const int32_t delta = static_cast<int32_t>(slotStartMillis_ - nowMillis);
  remainingMillis = delta > 0 ? static_cast<uint32_t>(delta) : 0;
  return true;
}

void WsprScheduler::disarm() {
  armed_ = false;
  authorization_ = WsprTimeAuthorization::NONE;
}

void WsprScheduler::startPreparedSymbols() {
  // Symbol 0 is already programmed before the slot and starts when CLK0 is
  // enabled. The next RF update is therefore symbol 1 at the original slot
  // deadline plus one complete WSPR symbol period.
  symbolIndex_ = 1;
  nextSymbolMicros_ = slotStartMicros_ + SYMBOL_PERIOD_US;
}

void WsprScheduler::startSymbols(uint32_t nowMicros) {
  symbolIndex_ = 0;
  nextSymbolMicros_ = nowMicros;
}

bool WsprScheduler::symbolDue(uint32_t nowMicros) const {
  return static_cast<int32_t>(nowMicros - nextSymbolMicros_) >= 0;
}

bool WsprScheduler::takeSymbol(uint16_t &index) {
  if (symbolsComplete()) return false;
  index = symbolIndex_++;
  nextSymbolMicros_ += SYMBOL_PERIOD_US;
  return true;
}
