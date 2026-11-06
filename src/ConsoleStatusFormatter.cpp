#include "ConsoleStatusFormatter.h"

#include <stdio.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <Arduino.h>
#define WSPR_FORMAT snprintf_P
#define WSPR_FORMAT_TEXT(value) PSTR(value)
#define WSPR_TEXT(value) PSTR(value)
#define WSPR_STRING_SPECIFIER "%S"
#define WSPR_STRING_TYPE "S"
#define WSPR_NO_SWITCH_TABLE __attribute__((optimize("no-tree-switch-conversion")))
#else
#define WSPR_FORMAT snprintf
#define WSPR_FORMAT_TEXT(value) value
#define WSPR_TEXT(value) value
#define WSPR_STRING_SPECIFIER "%s"
#define WSPR_STRING_TYPE "s"
#define WSPR_NO_SWITCH_TABLE
#endif

namespace {
constexpr size_t LINE_CAPACITY = 112;

WSPR_NO_SWITCH_TABLE const char *timeSourceLabel(TimeSource source) {
  switch (source) {
    case TimeSource::NONE: return WSPR_TEXT("NONE");
    case TimeSource::GPS: return WSPR_TEXT("GPS");
    case TimeSource::NTP: return WSPR_TEXT("NTP");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *timeQualityLabel(TimeQuality quality) {
  switch (quality) {
    case TimeQuality::UNKNOWN: return WSPR_TEXT("UNKNOWN");
    case TimeQuality::COARSE: return WSPR_TEXT("COARSE");
    case TimeQuality::SYNCHRONIZED: return WSPR_TEXT("SYNCHRONIZED");
    case TimeQuality::PRECISE: return WSPR_TEXT("PRECISE");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *authorizationLabel(
    WsprTimeAuthorization authorization) {
  switch (authorization) {
    case WsprTimeAuthorization::NONE: return WSPR_TEXT("INHIBITED");
    case WsprTimeAuthorization::GPS_PRECISE: return WSPR_TEXT("GPS_PRECISE");
    case WsprTimeAuthorization::NTP_EXPERIMENTAL: return WSPR_TEXT("NTP_EXPERIMENTAL");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *locatorSourceLabel(LocatorSource source) {
  switch (source) {
    case LocatorSource::FIXED: return WSPR_TEXT("FIXED");
    case LocatorSource::GPS_LIVE: return WSPR_TEXT("GPS_LIVE");
    case LocatorSource::GPS_LAST_KNOWN: return WSPR_TEXT("GPS_LAST_KNOWN");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *txStateLabel(ConsoleTxState state) {
  switch (state) {
    case ConsoleTxState::RF_INHIBITED: return WSPR_TEXT("RF_INHIBITED");
    case ConsoleTxState::WAITING_FOR_TIME: return WSPR_TEXT("WAIT_TIME");
    case ConsoleTxState::IDLE: return WSPR_TEXT("IDLE");
    case ConsoleTxState::PREPARING: return WSPR_TEXT("PREPARING");
    case ConsoleTxState::TRANSMITTING: return WSPR_TEXT("TRANSMITTING");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *inhibitLabel(TxInhibitReason reason) {
  switch (reason) {
    case TxInhibitReason::NONE: return WSPR_TEXT("NONE");
    case TxInhibitReason::RF_UNAVAILABLE: return WSPR_TEXT("RF_UNAVAILABLE");
    case TxInhibitReason::TIME_UNAVAILABLE: return WSPR_TEXT("TIME_UNAVAILABLE");
    case TxInhibitReason::TIME_UNQUALIFIED: return WSPR_TEXT("TIME_UNQUALIFIED");
    case TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED:
      return WSPR_TEXT("OPERATING_IDENTITY_UNCONFIGURED");
    case TxInhibitReason::MANUAL_TX_OFF: return WSPR_TEXT("MANUAL_TX_OFF");
    case TxInhibitReason::SLOT_MISSED: return WSPR_TEXT("SLOT_MISSED");
    case TxInhibitReason::RF_RUNTIME_ERROR: return WSPR_TEXT("RF_RUNTIME_ERROR");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *wifiStateLabel(WifiConnectionState state) {
  switch (state) {
    case WifiConnectionState::STATE_DISABLED: return WSPR_TEXT("DISABLED");
    case WifiConnectionState::STATE_DISCONNECTED: return WSPR_TEXT("DISCONNECTED");
    case WifiConnectionState::STATE_CONNECTING: return WSPR_TEXT("CONNECTING");
    case WifiConnectionState::STATE_CONNECTED: return WSPR_TEXT("CONNECTED");
    case WifiConnectionState::STATE_FAILED: return WSPR_TEXT("FAILED");
    case WifiConnectionState::STATE_AP_OPERATIONAL:
      return WSPR_TEXT("AP_OPERATIONAL");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *httpGateReasonLabel(
    HttpGateCloseReason reason) {
  switch (reason) {
    case HttpGateCloseReason::NONE: return WSPR_TEXT("NONE");
    case HttpGateCloseReason::WIFI_DISCONNECTED:
      return WSPR_TEXT("WIFI_DISCONNECTED");
    case HttpGateCloseReason::RF_CRITICAL: return WSPR_TEXT("RF_CRITICAL");
    case HttpGateCloseReason::SLOT_GUARD: return WSPR_TEXT("SLOT_GUARD");
    case HttpGateCloseReason::TIME_UNCERTAIN:
      return WSPR_TEXT("TIME_UNCERTAIN");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *diagnosticStateLabel(DiagnosticState state) {
  switch (state) {
    case DiagnosticState::STATE_UNKNOWN: return WSPR_TEXT("UNKNOWN");
    case DiagnosticState::STATE_DISABLED: return WSPR_TEXT("DISABLED");
    case DiagnosticState::STATE_NOT_DETECTED: return WSPR_TEXT("NOT_DETECTED");
    case DiagnosticState::STATE_INITIALIZING: return WSPR_TEXT("INITIALIZING");
    case DiagnosticState::STATE_OPERATIONAL: return WSPR_TEXT("OPERATIONAL");
    case DiagnosticState::STATE_DEGRADED: return WSPR_TEXT("DEGRADED");
    case DiagnosticState::STATE_ERROR: return WSPR_TEXT("ERROR");
  }
  return WSPR_TEXT("--");
}

WSPR_NO_SWITCH_TABLE const char *diagnosticReasonLabel(DiagnosticReason reason) {
  switch (reason) {
    case DiagnosticReason::NONE: return WSPR_TEXT("NONE");
    case DiagnosticReason::NOT_EVALUATED: return WSPR_TEXT("NOT_EVALUATED");
    case DiagnosticReason::DISABLED_BY_CONFIGURATION: return WSPR_TEXT("DISABLED_BY_CONFIG");
    case DiagnosticReason::DEVICE_NOT_FOUND: return WSPR_TEXT("DEVICE_NOT_FOUND");
    case DiagnosticReason::INITIALIZATION_FAILED: return WSPR_TEXT("INIT_FAILED");
    case DiagnosticReason::COMMUNICATION_FAILURE: return WSPR_TEXT("COMM_FAILURE");
    case DiagnosticReason::TIMEOUT: return WSPR_TEXT("TIMEOUT");
    case DiagnosticReason::INVALID_DATA: return WSPR_TEXT("INVALID_DATA");
    case DiagnosticReason::SERVICE_UNAVAILABLE: return WSPR_TEXT("SERVICE_UNAVAILABLE");
    case DiagnosticReason::WAITING_FOR_DATA: return WSPR_TEXT("WAITING_FOR_DATA");
    case DiagnosticReason::NOT_IMPLEMENTED: return WSPR_TEXT("NOT_IMPLEMENTED");
    case DiagnosticReason::RF_OUTPUT_UNVERIFIED: return WSPR_TEXT("RF_UNVERIFIED");
    case DiagnosticReason::INTERNAL_ERROR: return WSPR_TEXT("INTERNAL_ERROR");
  }
  return WSPR_TEXT("--");
}

void writeLine(ConsoleStatusFormatter::LineWriter writer, void *context,
               const char *line) {
  writer(context, line);
}

void writeText(ConsoleStatusFormatter::LineWriter writer, void *context,
               const char *text) {
#if defined(ARDUINO_ARCH_ESP8266)
  char line[LINE_CAPACITY];
  strncpy_P(line, text, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  writer(context, line);
#else
  writer(context, text);
#endif
}

void formatUtc(char *destination, size_t capacity, uint32_t millisOfDay) {
  const uint32_t seconds = millisOfDay / 1000UL;
  WSPR_FORMAT(destination, capacity, WSPR_FORMAT_TEXT("%02lu:%02lu:%02lu"),
           static_cast<unsigned long>(seconds / 3600UL),
           static_cast<unsigned long>((seconds / 60UL) % 60UL),
           static_cast<unsigned long>(seconds % 60UL));
}

void formatDuration(char *destination, size_t capacity, uint32_t millis) {
  WSPR_FORMAT(destination, capacity, WSPR_FORMAT_TEXT("%lu.%03lu s"),
           static_cast<unsigned long>(millis / 1000UL),
           static_cast<unsigned long>(millis % 1000UL));
}

void formatUptime(char *destination, size_t capacity, uint32_t millis) {
  const uint32_t totalSeconds = millis / 1000UL;
  const uint32_t days = totalSeconds / 86400UL;
  const uint32_t hours = (totalSeconds / 3600UL) % 24UL;
  const uint32_t minutes = (totalSeconds / 60UL) % 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  if (days == 0) {
    WSPR_FORMAT(destination, capacity,
             WSPR_FORMAT_TEXT("%02lu:%02lu:%02lu.%03lu"),
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(millis % 1000UL));
  } else {
    WSPR_FORMAT(destination, capacity,
             WSPR_FORMAT_TEXT("%lud %02lu:%02lu:%02lu.%03lu"),
             static_cast<unsigned long>(days),
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(millis % 1000UL));
  }
}

void writeModule(ConsoleStatusFormatter::LineWriter writer, void *context,
                 const char *label, const char *prefix,
                 const DiagnosticRecord &record, uint32_t uptimeMillis) {
  char line[LINE_CAPACITY];
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" %-12" WSPR_STRING_TYPE ": "
                            WSPR_STRING_SPECIFIER " ("
                            WSPR_STRING_SPECIFIER ")"), label,
           diagnosticStateLabel(record.state),
           diagnosticReasonLabel(record.reason));
  writeLine(writer, context, line);

  char age[24];
  formatDuration(age, sizeof(age),
                 static_cast<uint32_t>(uptimeMillis - record.lastChangeMillis));
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" %-12" WSPR_STRING_TYPE ": %s"), prefix,
           age);
  writeLine(writer, context, line);

  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" %-12" WSPR_STRING_TYPE
                            ": %u changes / %u ok / %u errors"),
           WSPR_TEXT("Counters"), record.counters.stateChanges,
           record.counters.successes, record.counters.failures);
  writeLine(writer, context, line);
}
}  // namespace

bool ConsoleStatusFormatter::outputAllowed(ConsoleTxState state) {
  return state != ConsoleTxState::PREPARING &&
         state != ConsoleTxState::TRANSMITTING;
}

void ConsoleStatusFormatter::writeSummary(
    const ConsoleStatusSnapshot &snapshot, const char *boardName,
    LineWriter writer, void *context) {
  if (writer == nullptr) return;
  char line[LINE_CAPACITY];
  char value[32];

  writeText(writer, context, WSPR_TEXT("================================================"));
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" WSPR BEACON 2.0 | %s"),
           boardName == nullptr ? "--" : boardName);
  writeLine(writer, context, line);
  writeText(writer, context, WSPR_TEXT("================================================"));

  if (snapshot.time.utcValid()) formatUtc(value, sizeof(value), snapshot.time.utcMillisOfDay);
  else WSPR_FORMAT(value, sizeof(value), WSPR_FORMAT_TEXT("--"));
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" UTC          : %s"), value);
  writeLine(writer, context, line);
  formatUptime(value, sizeof(value), snapshot.uptimeMillis);
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Uptime       : %s"), value);
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Console      : " WSPR_STRING_SPECIFIER),
           snapshot.monitorEnabled() ? WSPR_TEXT("MONITOR")
                                     : WSPR_TEXT("INTERACTIVE"));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Candidate    : " WSPR_STRING_SPECIFIER),
           snapshot.candidateDirty() ? WSPR_TEXT("MODIFIED")
                                     : WSPR_TEXT("UNCHANGED"));
  writeLine(writer, context, line);
  const ConfigurationSessionSnapshot &mode = snapshot.operatingMode;
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Operating    : %s"),
              mode.mode == FirmwareOperatingMode::CONFIGURATION
                  ? "CONFIGURATION" : "NORMAL");
  writeLine(writer, context, line);
  const char *owners = mode.cliActive && mode.webActive
      ? "CLI+WEB" : (mode.cliActive ? "CLI" : (mode.webActive ? "WEB" : "NONE"));
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Sessions     : %s"), owners);
  writeLine(writer, context, line);
  const char *admission = mode.txAdmissionAllowed
      ? "ALLOWED" : (mode.resumeGuardActive ? "RESUME_GUARD" : "INHIBITED");
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" TX Admission : %s (rev %lu)"),
              admission, static_cast<unsigned long>(mode.revision));
  writeLine(writer, context, line);

  writeText(writer, context, WSPR_TEXT(""));
  writeText(writer, context, WSPR_TEXT(" [TIME]"));
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Source       : " WSPR_STRING_SPECIFIER),
           timeSourceLabel(snapshot.time.source));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Quality      : " WSPR_STRING_SPECIFIER),
           timeQualityLabel(snapshot.time.quality));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" TX Authority : " WSPR_STRING_SPECIFIER),
           authorizationLabel(snapshot.authorization));
  writeLine(writer, context, line);
  if (snapshot.time.utcValid()) {
    formatDuration(value, sizeof(value), snapshot.time.referenceAgeMillis);
  } else {
    WSPR_FORMAT(value, sizeof(value), WSPR_FORMAT_TEXT("--"));
  }
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Sync Age     : %s"), value);
  writeLine(writer, context, line);

  writeText(writer, context, WSPR_TEXT(""));
  writeText(writer, context, WSPR_TEXT(" [LOCATION]"));
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Locator      : %s"), snapshot.location.locator);
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Source       : " WSPR_STRING_SPECIFIER),
           locatorSourceLabel(snapshot.location.source));
  writeLine(writer, context, line);

  writeText(writer, context, WSPR_TEXT(""));
  writeText(writer, context, WSPR_TEXT(" [RADIO]"));
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" State        : " WSPR_STRING_SPECIFIER),
           txStateLabel(snapshot.txState));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Mode         : %s"), wsprFrequencyModeLabel(snapshot.frequencyMode));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Config Freq  : %lu.%06lu MHz"),
           static_cast<unsigned long>(snapshot.configuredFrequencyHz / 1000000UL),
           static_cast<unsigned long>(snapshot.configuredFrequencyHz % 1000000UL));
  writeLine(writer, context, line);
  if (snapshot.txTelemetry.lastFrame.valid) {
    const uint32_t frequency = snapshot.txTelemetry.lastFrame.frequencyHz;
    WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Last TX Freq : %lu.%06lu MHz"),
             static_cast<unsigned long>(frequency / 1000000UL),
             static_cast<unsigned long>(frequency % 1000000UL));
  } else {
    WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Last TX Freq : --"));
  }
  writeLine(writer, context, line);
  if (snapshot.nextSlotValid()) {
    formatUtc(value, sizeof(value), snapshot.nextSlotUtcMillis);
    WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Next Slot    : %s UTC"), value);
  } else {
    WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Next Slot    : --"));
  }
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" RF Inhibit   : " WSPR_STRING_SPECIFIER),
           inhibitLabel(snapshot.txTelemetry.inhibitReason));
  writeLine(writer, context, line);
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" Frames       : %lu started / %lu completed / %lu aborted"),
           static_cast<unsigned long>(snapshot.txTelemetry.framesStarted),
           static_cast<unsigned long>(snapshot.txTelemetry.framesCompleted),
           static_cast<unsigned long>(snapshot.txTelemetry.framesInterrupted));
  writeLine(writer, context, line);

  writeText(writer, context, WSPR_TEXT(""));
  writeText(writer, context, WSPR_TEXT(" [HARDWARE]"));
  writeModule(writer, context, WSPR_TEXT("Si5351A"), WSPR_TEXT("Age"),
              snapshot.diagnostics.modules[2], snapshot.uptimeMillis);

  writeText(writer, context, WSPR_TEXT(""));
  writeText(writer, context, WSPR_TEXT(" [NETWORK]"));
  WSPR_FORMAT(line, sizeof(line),
           WSPR_FORMAT_TEXT(" WiFi         : " WSPR_STRING_SPECIFIER
                            " (" WSPR_STRING_SPECIFIER ")"),
           wifiStateLabel(snapshot.wifiState),
           snapshot.wifiSaved() ? WSPR_TEXT("SAVED") : WSPR_TEXT("NOT_SAVED"));
  writeLine(writer, context, line);
  writeModule(writer, context, WSPR_TEXT("WiFi Diag"), WSPR_TEXT("WiFi Age"),
              snapshot.diagnostics.modules[4], snapshot.uptimeMillis);
  writeModule(writer, context, WSPR_TEXT("NTP"), WSPR_TEXT("NTP Age"),
              snapshot.diagnostics.modules[5], snapshot.uptimeMillis);

  if (snapshot.http.available()) {
    writeText(writer, context, WSPR_TEXT(""));
    writeText(writer, context, WSPR_TEXT(" [HTTP]"));
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Gate         : " WSPR_STRING_SPECIFIER
                              " (" WSPR_STRING_SPECIFIER ")"),
             snapshot.http.gateOpen() ? WSPR_TEXT("OPEN") : WSPR_TEXT("CLOSED"),
             httpGateReasonLabel(snapshot.http.gateReason));
    writeLine(writer, context, line);
    uint32_t gateAgeMillis = 0;
    if (snapshot.http.gateAgeMillis(snapshot.uptimeMillis, gateAgeMillis)) {
      formatDuration(value, sizeof(value), gateAgeMillis);
    } else {
      WSPR_FORMAT(value, sizeof(value), WSPR_FORMAT_TEXT("--"));
    }
    WSPR_FORMAT(line, sizeof(line), WSPR_FORMAT_TEXT(" Gate Age     : %s"), value);
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Gate Count   : %u open / %u closed"),
             snapshot.http.gateOpenings, snapshot.http.gateClosures);
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Listener     : intended " WSPR_STRING_SPECIFIER),
             snapshot.http.listenerIntended() ? WSPR_TEXT("ON")
                                              : WSPR_TEXT("OFF"));
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Clients      : %u accepted"),
             snapshot.http.clientsAccepted);
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Responses    : %u served / %u timeout (%u req / %u resp)"),
             snapshot.http.responsesServed, snapshot.http.clientTimeouts,
             snapshot.http.requestTimeouts, snapshot.http.responseTimeouts);
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Failures     : %u request / %u forced close"),
             snapshot.http.failedRequests, snapshot.http.forcedClosures);
    writeLine(writer, context, line);
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Transfer     : %lu bytes / %u partial / %u zero"),
             static_cast<unsigned long>(snapshot.http.bytesSent),
             snapshot.http.partialWrites, snapshot.http.zeroWrites);
    writeLine(writer, context, line);
#if defined(ARDUINO_ARCH_ESP32)
    writeText(writer, context,
              WSPR_TEXT(" TCP capacity : unavailable on ESP32"));
#else
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" TCP capacity : %u zero-capacity turns"),
             snapshot.http.noCapacityTurns);
    writeLine(writer, context, line);
#endif
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Latency max  : write %u ms / response %u ms"),
             snapshot.http.maxWriteMillis, snapshot.http.maxResponseMillis);
    writeLine(writer, context, line);
    if (snapshot.http.lastResponseValid()) {
      WSPR_FORMAT(line, sizeof(line),
               WSPR_FORMAT_TEXT(" Last response: %u ms / %u bytes / "
                                WSPR_STRING_SPECIFIER),
               snapshot.http.lastResponseMillis,
               snapshot.http.lastResponseBytes,
               snapshot.http.lastResponseComplete()
                   ? WSPR_TEXT("complete") : WSPR_TEXT("interrupted"));
      writeLine(writer, context, line);
    }
    WSPR_FORMAT(line, sizeof(line),
             WSPR_FORMAT_TEXT(" Closures     : %u gate / %u disconnect / %u interrupted"),
             snapshot.http.gateClientClosures,
             snapshot.http.disconnectClosures,
             snapshot.http.responsesInterrupted);
    writeLine(writer, context, line);
  }
  writeText(writer, context, WSPR_TEXT("================================================"));
}
