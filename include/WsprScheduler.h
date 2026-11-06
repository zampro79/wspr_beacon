#pragma once

#include <stdint.h>

#include "TimeService.h"

enum class WsprStartDecision : uint8_t { WAIT, START, MISSED };
enum class WsprTimeAuthorization : uint8_t {
  NONE,
  GPS_PRECISE,
  NTP_EXPERIMENTAL
};

class WsprScheduler {
 public:
  static constexpr uint16_t SYMBOL_COUNT = 162;
  static constexpr uint32_t SYMBOL_PERIOD_US = 682667UL;
  static constexpr uint32_t START_WINDOW_MS = 2000UL;

  static WsprTimeAuthorization authorizationFor(const TimeSnapshot &time);
  static bool nextSlotUtcMillis(const TimeSnapshot &time,
                               uint8_t intervalMinutes,
                               uint32_t &nextSlotUtcMillis);
  static bool millisUntilNextSlot(const TimeSnapshot &time,
                                  uint8_t intervalMinutes,
                                  uint32_t &remainingMillis);
  static bool transmissionAuthorized(const TimeSnapshot &time) {
    return authorizationFor(time) != WsprTimeAuthorization::NONE;
  }

  bool arm(const TimeSnapshot &time, uint32_t nowMillis, uint32_t nowMicros,
           uint8_t intervalMinutes, uint8_t preparationSeconds);
  bool arm(const TimeSnapshot &time, uint32_t nowMillis,
           uint8_t intervalMinutes, uint8_t preparationSeconds) {
    return arm(time, nowMillis, nowMillis * 1000UL,
               intervalMinutes, preparationSeconds);
  }
  WsprStartDecision startDecision(uint32_t nowMillis) const;
  bool millisUntilSlot(uint32_t nowMillis, uint32_t &remainingMillis) const;
  void disarm();
  bool armed() const { return armed_; }
  uint32_t slot() const { return slot_; }
  WsprTimeAuthorization authorization() const { return authorization_; }

  void startPreparedSymbols();
  void startSymbols(uint32_t nowMicros);
  bool symbolDue(uint32_t nowMicros) const;
  bool takeSymbol(uint16_t &index);
  bool symbolsComplete() const { return symbolIndex_ >= SYMBOL_COUNT; }

 private:
  uint32_t slotStartMillis_ = 0;
  uint32_t slotStartMicros_ = 0;
  uint32_t slot_ = UINT32_MAX;
  uint32_t nextSymbolMicros_ = 0;
  uint16_t symbolIndex_ = 0;
  WsprTimeAuthorization authorization_ = WsprTimeAuthorization::NONE;
  bool armed_ = false;
};
