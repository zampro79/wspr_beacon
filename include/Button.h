#pragma once

#include <Arduino.h>

class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, INPUT_PULLUP);
    stableState_ = rawState_ = digitalRead(pin_);
  }

  bool pressed() {
    const bool sample = digitalRead(pin_);
    const uint32_t now = millis();
    if (sample != rawState_) {
      rawState_ = sample;
      changedAt_ = now;
    }
    if (sample != stableState_ && now - changedAt_ >= 30) {
      stableState_ = sample;
      return stableState_ == LOW;
    }
    return false;
  }

 private:
  uint8_t pin_;
  bool rawState_ = HIGH;
  bool stableState_ = HIGH;
  uint32_t changedAt_ = 0;
};
