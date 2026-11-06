#pragma once

#include <stdint.h>

#include "DiagnosticRegistry.h"
#include "ConfigurationSessionController.h"
#include "LocatorService.h"
#include "TimeService.h"
#include "TxTelemetry.h"
#include "WifiConnectionTracker.h"
#include "WsprFrequencySelector.h"
#include "WsprScheduler.h"

enum class ApplicationTxState : uint8_t {
  RF_INHIBITED,
  WAITING_FOR_TIME,
  IDLE,
  PREPARING,
  TRANSMITTING
};

struct ApplicationGpsSnapshot {
  int32_t latitudeMicrodegrees = 0;
  int32_t longitudeMicrodegrees = 0;
  uint8_t satellites = 0;
  bool nmeaAvailable = false;
  bool fixValid = false;
  bool ppsPresent = false;
  bool ppsAssociated = false;
};

struct ApplicationStatusSnapshot {
  static constexpr uint8_t SCHEMA_VERSION = 1;

  uint32_t uptimeMillis = 0;
  uint32_t configuredFrequencyHz = 0;
  uint32_t selectedFrequencyHz = 0;
  uint32_t nextSlotUtcMillis = 0;
  uint32_t remainingMillis = 0;
  uint32_t freeHeapBytes = 0;
  char callsign[7] = {};
  char locator[5] = {};
  char band[5] = {};
  char ipv4[16] = {};
  char board[16] = {};
  uint8_t powerDbm = 0;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
  LocatorSource locatorSource = LocatorSource::FIXED;
  ApplicationTxState txState = ApplicationTxState::WAITING_FOR_TIME;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  WifiConnectionState wifiState = WifiConnectionState::STATE_DISABLED;
  TimeSnapshot time;
  TxTelemetrySnapshot telemetry;
  DiagnosticSnapshot diagnostics;
  ApplicationGpsSnapshot gps;
  ConfigurationSessionSnapshot operatingMode;
  bool rfReady = false;
  bool selectedFrequencyValid = false;
  bool nextSlotValid = false;
  bool freeHeapValid = false;
  bool manualTxPermitted = false;
  bool effectiveTxEligible = false;
};

static_assert(sizeof(ApplicationStatusSnapshot) <= 256,
              "Application status snapshot must remain compact");
