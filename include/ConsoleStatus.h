#pragma once

#include <stdint.h>

#include "DiagnosticRegistry.h"
#include "ConfigurationSessionController.h"
#include "HttpDiagnostics.h"
#include "LocatorService.h"
#include "TimeService.h"
#include "TxTelemetry.h"
#include "WifiConnectionTracker.h"
#include "WsprScheduler.h"
#include "WsprFrequencySelector.h"

enum class ConsoleTxState : uint8_t {
  RF_INHIBITED,
  WAITING_FOR_TIME,
  IDLE,
  PREPARING,
  TRANSMITTING
};

struct ConsoleStatusInputs {
  uint32_t nowMillis = 0;
  uint32_t configuredFrequencyHz = 0;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  uint8_t transmitIntervalMinutes = 0;
  TimeSnapshot time;
  LocatorSnapshot location;
  DiagnosticSnapshot diagnostics;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  WifiConnectionState wifiState = WifiConnectionState::STATE_DISABLED;
  ConsoleTxState txState = ConsoleTxState::WAITING_FOR_TIME;
  TxTelemetrySnapshot txTelemetry;
  HttpDiagnosticsSnapshot http;
  ConfigurationSessionSnapshot operatingMode;
  bool monitorEnabled = false;
  bool candidateDirty = false;
  bool wifiSaved = false;
  bool gpsUtcValid = false;
  bool gpsPositionValid = false;
  bool ppsPresent = false;
  bool ppsAssociated = false;
};

struct ConsoleStatusSnapshot {
  uint32_t uptimeMillis = 0;
  uint32_t configuredFrequencyHz = 0;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  uint32_t nextSlotUtcMillis = 0;
  TimeSnapshot time;
  LocatorSnapshot location;
  DiagnosticSnapshot diagnostics;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  WifiConnectionState wifiState = WifiConnectionState::STATE_DISABLED;
  ConsoleTxState txState = ConsoleTxState::WAITING_FOR_TIME;
  TxTelemetrySnapshot txTelemetry;
  HttpDiagnosticsSnapshot http;
  ConfigurationSessionSnapshot operatingMode;
  uint8_t flags = 0;

  bool nextSlotValid() const { return (flags & (1U << 0)) != 0; }
  bool monitorEnabled() const { return (flags & (1U << 1)) != 0; }
  bool candidateDirty() const { return (flags & (1U << 2)) != 0; }
  bool wifiSaved() const { return (flags & (1U << 3)) != 0; }
  bool gpsUtcValid() const { return (flags & (1U << 4)) != 0; }
  bool gpsPositionValid() const { return (flags & (1U << 5)) != 0; }
  bool ppsPresent() const { return (flags & (1U << 6)) != 0; }
  bool ppsAssociated() const { return (flags & (1U << 7)) != 0; }
};

class ConsoleStatus {
 public:
  static ConsoleStatusSnapshot build(const ConsoleStatusInputs &inputs);
};

static_assert(sizeof(ConsoleStatusSnapshot) <= 264,
              "Console status snapshot must remain compact");
