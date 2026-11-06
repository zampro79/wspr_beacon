#pragma once

#include <stdint.h>

enum class PowerSavePresentation : uint8_t {
  NONE,
  STA_IPV4,
  AP_PROVISIONING,
  AP_OPERATIONAL,
  AP_START_ERROR
};

enum class PowerSaveDisplayAction : uint8_t { NONE, DISPLAY_OFF, DISPLAY_ON };

class PowerSaveDisplayPolicy {
 public:
  void begin(bool enabled, uint32_t nowMillis);
  void setEnabled(bool enabled, uint32_t nowMillis);
  void setPresentation(PowerSavePresentation presentation,
                       uint32_t nowMillis);
  PowerSaveDisplayAction update(uint32_t nowMillis, bool timingCritical,
                                bool displayAvailable);

  bool enabled() const { return enabled_; }
  bool displayOff() const { return displayOff_; }
  bool renderAllowed() const { return !displayOff_ && !immediateOffPending_; }

 private:
  bool enabled_ = false;
  bool displayOff_ = false;
  bool immediateOffPending_ = false;
  bool onPending_ = false;
  uint32_t lastMillis_ = 0;
  uint32_t visibleMillis_ = 0;
  PowerSavePresentation target_ = PowerSavePresentation::NONE;
  PowerSavePresentation visible_ = PowerSavePresentation::NONE;

  static uint32_t requiredMillis(PowerSavePresentation presentation);
  void account(uint32_t nowMillis, bool timingCritical);
};
