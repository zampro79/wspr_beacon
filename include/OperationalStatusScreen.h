#pragma once

#include <stdint.h>

#include "LocatorService.h"
#include "TimeService.h"
#include "WifiConnectionTracker.h"
#include "WsprScheduler.h"

enum class WifiDisplayMode : uint8_t { OFF, STA, AP };

struct OperationalStatusInputs {
  uint32_t nowMillis = 0;
  TimeSnapshot time;
  LocatorSnapshot locator;
  WsprTimeAuthorization authorization = WsprTimeAuthorization::NONE;
  WifiConnectionState wifiState = WifiConnectionState::STATE_DISABLED;
  WifiDisplayMode wifiMode = WifiDisplayMode::OFF;
  const char *wifiIpv4 = nullptr;
  float latitude = 0;
  float longitude = 0;
  uint8_t satellites = 0;
  bool gpsNmeaAvailable = false;
  bool gpsFixValid = false;
  bool wifiReconnecting = false;
};

struct OperationalStatusSnapshot {
  static constexpr uint8_t LINE_COUNT = 5;
  static constexpr uint8_t MAX_CHARACTERS = 21;
  char lines[LINE_COUNT][MAX_CHARACTERS + 1] = {};
};

class OperationalStatusScreen {
 public:
  static constexpr uint32_t CONNECTED_LABEL_MS = 3000UL;

  OperationalStatusSnapshot update(const OperationalStatusInputs &inputs);
  bool ipv4Visible() const { return ipv4Visible_; }

 private:
  WifiConnectionState previousWifiState_ =
      WifiConnectionState::STATE_DISABLED;
  uint32_t connectedSinceMillis_ = 0;
  bool connectedObserved_ = false;
  bool ipv4Visible_ = false;

  static const char *modeLabel(WifiDisplayMode mode);
};

static_assert(sizeof(OperationalStatusSnapshot) == 110,
              "Operational OLED snapshot size changed");
