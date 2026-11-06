#include "OperationalStatusScreen.h"

#include <stdio.h>
#include <string.h>

namespace {
void formatCoordinate(char *destination, size_t size, float value,
                      bool latitude) {
  const bool negative = value < 0;
  const float absolute = negative ? -value : value;
  const uint32_t thousandths =
      static_cast<uint32_t>(absolute * 1000.0F + 0.5F);
  const uint32_t degrees = thousandths / 1000UL;
  const uint32_t fraction = thousandths % 1000UL;
  const char hemisphere = latitude ? (negative ? 'S' : 'N')
                                   : (negative ? 'W' : 'E');
  snprintf(destination, size, latitude ? "%02lu.%03lu%c" : "%03lu.%03lu%c",
           static_cast<unsigned long>(degrees),
           static_cast<unsigned long>(fraction), hemisphere);
}

const char *locatorSourceLabel(LocatorSource source) {
  switch (source) {
    case LocatorSource::FIXED: return "DEF";
    case LocatorSource::GPS_LIVE: return "GPS";
    case LocatorSource::GPS_LAST_KNOWN: return "LAST";
  }
  return "DEF";
}
}  // namespace

const char *OperationalStatusScreen::modeLabel(WifiDisplayMode mode) {
  if (mode == WifiDisplayMode::STA) return "STA";
  if (mode == WifiDisplayMode::AP) return "AP";
  return "";
}

OperationalStatusSnapshot OperationalStatusScreen::update(
    const OperationalStatusInputs &inputs) {
  OperationalStatusSnapshot result;
  ipv4Visible_ = false;

  const bool hasPreviousFix =
      inputs.locator.source == LocatorSource::GPS_LAST_KNOWN;
  const char *gpsState = inputs.gpsFixValid
      ? "FIX" : (hasPreviousFix ? "LOST" : "SEARCH");
  if (inputs.gpsNmeaAvailable) {
    snprintf(result.lines[0], sizeof(result.lines[0]), "GPS: %s SAT:%02u",
             gpsState, inputs.satellites > 99 ? 99 : inputs.satellites);
  } else {
    snprintf(result.lines[0], sizeof(result.lines[0]), "GPS: %s SAT:--",
             gpsState);
  }

  if (inputs.time.utcValid()) {
    const uint32_t seconds = inputs.time.utcMillisOfDay / 1000UL;
    const uint8_t hour = seconds / 3600UL;
    const uint8_t minute = (seconds / 60UL) % 60UL;
    const uint8_t second = seconds % 60UL;
    const char *source = inputs.time.source == TimeSource::GPS ? "GPS" : "NTP";
    const char qualification =
        inputs.authorization == WsprTimeAuthorization::NONE ? '!' : '*';
    snprintf(result.lines[1], sizeof(result.lines[1]),
             "UTC: %02u:%02u:%02u %s %c", hour, minute, second, source,
             qualification);
  } else {
    strcpy(result.lines[1], "UTC: --:--:-- --- -");
  }

  if (inputs.gpsFixValid || hasPreviousFix) {
    char latitude[9];
    char longitude[10];
    formatCoordinate(latitude, sizeof(latitude), inputs.latitude, true);
    formatCoordinate(longitude, sizeof(longitude), inputs.longitude, false);
    strcpy(result.lines[2], latitude);
    strcat(result.lines[2], " ");
    strcat(result.lines[2], longitude);
    if (hasPreviousFix) strcat(result.lines[2], " OLD");
  } else {
    strcpy(result.lines[2], "COORD: NOT AVAILABLE");
  }

  snprintf(result.lines[3], sizeof(result.lines[3]), "LOC: %.4s (%s)",
           inputs.locator.locator, locatorSourceLabel(inputs.locator.source));

  if (inputs.wifiState == WifiConnectionState::STATE_CONNECTED) {
    if (!connectedObserved_ ||
        previousWifiState_ != WifiConnectionState::STATE_CONNECTED) {
      connectedSinceMillis_ = inputs.nowMillis;
      connectedObserved_ = true;
    }
  } else {
    connectedObserved_ = false;
  }
  previousWifiState_ = inputs.wifiState;

  const char *mode = modeLabel(inputs.wifiMode);
  switch (inputs.wifiState) {
    case WifiConnectionState::STATE_DISABLED:
      strcpy(result.lines[4], "WiFi: OFF");
      break;
    case WifiConnectionState::STATE_CONNECTING:
      snprintf(result.lines[4], sizeof(result.lines[4]),
               inputs.wifiReconnecting ? "W:RECONNECTING (%s)"
                                       : "WiFi:CONNECTING (%s)", mode);
      break;
    case WifiConnectionState::STATE_CONNECTED:
      if (static_cast<uint32_t>(inputs.nowMillis - connectedSinceMillis_) <
          CONNECTED_LABEL_MS) {
        snprintf(result.lines[4], sizeof(result.lines[4]),
                 "WiFi: CONNECTED (%s)", mode);
      } else if (inputs.wifiIpv4 != nullptr && inputs.wifiIpv4[0] != '\0') {
        ipv4Visible_ = true;
        const size_t addressLength = strlen(inputs.wifiIpv4);
        if (addressLength + strlen(mode) + 8U <=
            OperationalStatusSnapshot::MAX_CHARACTERS) {
          snprintf(result.lines[4], sizeof(result.lines[4]), "WiFi: %s (%s)",
                   inputs.wifiIpv4, mode);
        } else {
          snprintf(result.lines[4], sizeof(result.lines[4]), "%s(%s)",
                   inputs.wifiIpv4, mode);
        }
      } else {
        snprintf(result.lines[4], sizeof(result.lines[4]),
                 "WiFi: CONNECTED (%s)", mode);
      }
      break;
    case WifiConnectionState::STATE_DISCONNECTED:
    case WifiConnectionState::STATE_FAILED:
      snprintf(result.lines[4], sizeof(result.lines[4]),
               "W:DISCONNECTED (%s)", mode);
      break;
    case WifiConnectionState::STATE_AP_OPERATIONAL:
      strcpy(result.lines[4], "WiFi: AP OPERATIONAL");
      break;
  }
  return result;
}
