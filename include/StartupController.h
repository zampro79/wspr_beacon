#pragma once

#include <stdint.h>

class StartupController {
 public:
  static constexpr uint32_t DURATION_MS = 5000UL;

  void begin(uint32_t monotonicMillis);
  bool active(uint32_t monotonicMillis) const;
  bool transmissionAllowed(uint32_t monotonicMillis) const {
    return !active(monotonicMillis);
  }

 private:
  uint32_t startedAtMillis_ = 0;
  bool started_ = false;
};
