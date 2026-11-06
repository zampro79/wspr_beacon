#pragma once

#include <stdint.h>

enum class StatusLedMode : uint8_t {
  OFF,
  STARTUP,
  HARDWARE_TEST,
  WAITING_SYNC,
  READY,
  WIFI_CONFIG,
  PREPARING,
  TRANSMITTING,
  RF_ERROR
};

struct StatusLedInputs {
  bool startupActive = false;
  bool hardwareTestActive = false;
  bool rfError = false;
  bool transmitting = false;
  bool preparing = false;
  bool wifiConfiguring = false;
  bool ready = false;
};

struct StatusLedOutput {
  bool enabled = false;
  bool changed = false;
  StatusLedMode mode = StatusLedMode::OFF;
};

class StatusLedController {
 public:
  void begin(uint32_t monotonicMillis);
  StatusLedOutput update(uint32_t monotonicMillis,
                         const StatusLedInputs &inputs);

 private:
  StatusLedMode mode_ = StatusLedMode::OFF;
  uint32_t modeStartedMillis_ = 0;
  bool outputEnabled_ = false;
  bool initialized_ = false;

  StatusLedMode selectMode(uint32_t monotonicMillis,
                           const StatusLedInputs &inputs) const;
  static bool pattern(StatusLedMode mode, uint32_t elapsedMillis);
};
