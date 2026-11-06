#include "PowerSaveDisplayPolicy.h"

uint32_t PowerSaveDisplayPolicy::requiredMillis(
    PowerSavePresentation presentation) {
  switch (presentation) {
    case PowerSavePresentation::STA_IPV4: return 15000UL;
    case PowerSavePresentation::AP_PROVISIONING: return 15000UL;
    case PowerSavePresentation::AP_OPERATIONAL: return 15000UL;
    case PowerSavePresentation::AP_START_ERROR: return 10000UL;
    case PowerSavePresentation::NONE: return 0;
  }
  return 0;
}

void PowerSaveDisplayPolicy::begin(bool enabled, uint32_t nowMillis) {
  enabled_ = enabled;
  displayOff_ = false;
  immediateOffPending_ = false;
  onPending_ = false;
  lastMillis_ = nowMillis;
  visibleMillis_ = 0;
  target_ = visible_ = PowerSavePresentation::NONE;
}

void PowerSaveDisplayPolicy::account(uint32_t nowMillis,
                                     bool timingCritical) {
  const uint32_t elapsed = nowMillis - lastMillis_;
  lastMillis_ = nowMillis;
  if (enabled_ && !displayOff_ && !timingCritical &&
      visible_ != PowerSavePresentation::NONE && visible_ == target_) {
    visibleMillis_ += elapsed;
  }
}

void PowerSaveDisplayPolicy::setEnabled(bool enabled, uint32_t nowMillis) {
  account(nowMillis, false);
  if (enabled == enabled_) return;
  enabled_ = enabled;
  if (enabled) {
    immediateOffPending_ = true;
    onPending_ = false;
  } else {
    immediateOffPending_ = false;
    onPending_ = displayOff_;
    visibleMillis_ = 0;
    target_ = PowerSavePresentation::NONE;
  }
}

void PowerSaveDisplayPolicy::setPresentation(
    PowerSavePresentation presentation, uint32_t nowMillis) {
  lastMillis_ = nowMillis;
  visible_ = presentation;
  if (presentation != PowerSavePresentation::NONE && presentation != target_) {
    target_ = presentation;
    visibleMillis_ = 0;
  }
}

PowerSaveDisplayAction PowerSaveDisplayPolicy::update(
    uint32_t nowMillis, bool timingCritical, bool displayAvailable) {
  account(nowMillis, timingCritical);
  if (!displayAvailable) {
    if (enabled_ && (immediateOffPending_ ||
        (target_ != PowerSavePresentation::NONE &&
         visibleMillis_ >= requiredMillis(target_)))) {
      displayOff_ = true;
      immediateOffPending_ = false;
    }
    return PowerSaveDisplayAction::NONE;
  }
  if (timingCritical) return PowerSaveDisplayAction::NONE;
  if (!enabled_ && onPending_) {
    onPending_ = false;
    displayOff_ = false;
    return PowerSaveDisplayAction::DISPLAY_ON;
  }
  if (enabled_ && !displayOff_ &&
      (immediateOffPending_ ||
       (target_ != PowerSavePresentation::NONE &&
        visibleMillis_ >= requiredMillis(target_)))) {
    immediateOffPending_ = false;
    displayOff_ = true;
    return PowerSaveDisplayAction::DISPLAY_OFF;
  }
  return PowerSaveDisplayAction::NONE;
}
