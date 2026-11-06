#pragma once

#include <stdint.h>

#include "TimeService.h"
#include "WsprFrequencySelector.h"

enum class OledRfState : uint8_t { READY, WAITING_TIME, INHIBITED, ERROR };

struct IdleStatusInputs {
  TimeSnapshot time;
  const char *callsign = nullptr;
  const char *locator = nullptr;
  const char *band = nullptr;
  uint32_t frequencyHz = 0;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  uint32_t nextSlotUtcMillis = 0;
  uint8_t powerDbm = 0;
  OledRfState rfState = OledRfState::INHIBITED;
  bool nextSlotValid = false;
};

struct IdleStatusSnapshot {
  static constexpr uint8_t LINE_COUNT = 5;
  static constexpr uint8_t MAX_CHARACTERS = 21;
  char lines[LINE_COUNT][MAX_CHARACTERS + 1] = {};
};

class IdleStatusScreen {
 public:
  static IdleStatusSnapshot build(const IdleStatusInputs &inputs);
};

static_assert(sizeof(IdleStatusSnapshot) == 110,
              "Idle OLED snapshot size changed");
