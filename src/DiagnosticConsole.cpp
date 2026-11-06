#include "DiagnosticConsole.h"

#include "BoardConfig.h"
#include "ConsoleStatusFormatter.h"

namespace {
ConsoleTxState consoleTxState(TransmitterState state) {
  switch (state) {
    case TransmitterState::RF_INHIBITED: return ConsoleTxState::RF_INHIBITED;
    case TransmitterState::WAITING_FOR_TIME: return ConsoleTxState::WAITING_FOR_TIME;
    case TransmitterState::IDLE: return ConsoleTxState::IDLE;
    case TransmitterState::PREPARING: return ConsoleTxState::PREPARING;
    case TransmitterState::TRANSMITTING: return ConsoleTxState::TRANSMITTING;
  }
  return ConsoleTxState::WAITING_FOR_TIME;
}
}  // namespace

void DiagnosticConsole::update(
    bool periodicEnabled, uint32_t nowMillis, const BeaconConfig &config,
    const GpsReceiver &gps, const TimeService &timeService,
    const LocatorSnapshot &locator, const WsprTransmitter &transmitter,
    const WifiManager &wifi, const ConfigurationManager *configuration,
    bool monitorEnabled, bool consoleCandidateDirty) {
  if (!periodicEnabled || !config.serialDiagnosticsEnabled ||
      config.serialDiagnosticsIntervalMs == 0) return;
  if (!ConsoleStatusFormatter::outputAllowed(
          consoleTxState(transmitter.state()))) return;
  if (static_cast<uint32_t>(nowMillis - lastSummaryMillis_) <
      config.serialDiagnosticsIntervalMs) return;
  lastSummaryMillis_ = nowMillis;
  printSummary(capture(nowMillis, config, gps, timeService, locator, transmitter,
                       wifi, configuration, monitorEnabled,
                       consoleCandidateDirty));
}

void DiagnosticConsole::printNow(
    uint32_t nowMillis, const BeaconConfig &config, const GpsReceiver &gps,
    const TimeService &timeService, const LocatorSnapshot &locator,
    const WsprTransmitter &transmitter, const WifiManager &wifi,
    const ConfigurationManager *configuration, bool monitorEnabled,
    bool consoleCandidateDirty, ConsoleStatusView view) {
  if (!ConsoleStatusFormatter::outputAllowed(
          consoleTxState(transmitter.state()))) return;
  const ConsoleStatusSnapshot snapshot =
      capture(nowMillis, config, gps, timeService, locator, transmitter, wifi,
              configuration, monitorEnabled, consoleCandidateDirty);
  switch (view) {
    case ConsoleStatusView::SUMMARY: printSummary(snapshot); break;
    case ConsoleStatusView::GPS: printGps(snapshot); break;
    case ConsoleStatusView::TIME: printTime(snapshot); break;
    case ConsoleStatusView::RF: printRadio(snapshot); break;
    case ConsoleStatusView::WIFI: printNetwork(snapshot); break;
    case ConsoleStatusView::MODULES: printModules(snapshot); break;
    case ConsoleStatusView::TX_STATUS: printTxStatus(snapshot); break;
    case ConsoleStatusView::TX_LAST: printTxLast(snapshot); break;
  }
}

ConsoleStatusSnapshot DiagnosticConsole::capture(
    uint32_t nowMillis, const BeaconConfig &config, const GpsReceiver &gps,
    const TimeService &timeService, const LocatorSnapshot &locator,
    const WsprTransmitter &transmitter, const WifiManager &wifi,
    const ConfigurationManager *configuration, bool monitorEnabled,
    bool consoleCandidateDirty) const {
  ConsoleStatusInputs inputs;
  inputs.nowMillis = nowMillis;
  inputs.configuredFrequencyHz = transmitter.configuredFrequencyHz();
  inputs.frequencyMode = transmitter.frequencyMode();
  inputs.transmitIntervalMinutes = config.transmitIntervalMinutes;
  inputs.time = timeService.snapshot(nowMillis);
  inputs.location = locator;
  diagnostics.snapshot(inputs.diagnostics);
  inputs.authorization = transmitter.timeAuthorization(inputs.time);
  inputs.wifiState = wifi.state();
  inputs.txState = consoleTxState(transmitter.state());
  inputs.txTelemetry = transmitter.telemetry();
  if (httpDiagnostics_ != nullptr) inputs.http = *httpDiagnostics_;
  if (configurationSessions_ != nullptr) {
    inputs.operatingMode = configurationSessions_->snapshot(nowMillis);
  }
  inputs.monitorEnabled = monitorEnabled;
  inputs.candidateDirty = consoleCandidateDirty;
  inputs.gpsUtcValid = gps.synchronized();
  inputs.gpsPositionValid = gps.data().positionValid;
  inputs.ppsPresent = gps.data().ppsAvailable;
  inputs.ppsAssociated = gps.data().ppsAssociated;
  if (configuration != nullptr) {
    const ConfigurationStatus status = configuration->status();
    inputs.candidateDirty = inputs.candidateDirty || status.wsprDirty || status.wifiDirty;
    inputs.wifiSaved = configuration->savedWifi().complete();
  }
  return ConsoleStatus::build(inputs);
}

void DiagnosticConsole::printSummary(const ConsoleStatusSnapshot &snapshot) {
  ConsoleStatusFormatter::writeSummary(snapshot, WSPR_BOARD_NAME,
                                       writeSummaryLine, this);
}

void DiagnosticConsole::writeSummaryLine(void *context, const char *line) {
  static_cast<DiagnosticConsole *>(context)->output_.println(line);
}

void DiagnosticConsole::printGps(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[GPS]"));
  output_.print(F("utc="));
  output_.print(snapshot.gpsUtcValid() ? F("VALID") : F("INVALID"));
  output_.print(F(" position="));
  output_.print(snapshot.gpsPositionValid() ? F("VALID") : F("INVALID"));
  output_.print(F(" pps="));
  output_.print(snapshot.ppsPresent() ? F("PRESENT") : F("ABSENT"));
  output_.print(F(" pps_utc="));
  output_.println(snapshot.ppsAssociated() ? F("ASSOCIATED") : F("UNASSOCIATED"));
  printModule(DiagnosticModule::GPS, snapshot.diagnostics.modules[0],
              snapshot.uptimeMillis);
  printModule(DiagnosticModule::PPS, snapshot.diagnostics.modules[1],
              snapshot.uptimeMillis);
}

void DiagnosticConsole::printTime(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[TIME]"));
  output_.print(F("utc="));
  if (snapshot.time.utcValid()) printUtc(snapshot.time.utcMillisOfDay);
  else output_.print(F("--:--:--"));
  output_.print(F(" source=")); printTimeSource(snapshot.time.source);
  output_.print(F(" quality=")); printTimeQuality(snapshot.time.quality);
  output_.print(F(" age_ms="));
  if (snapshot.time.utcValid()) output_.print(snapshot.time.referenceAgeMillis);
  else output_.print(F("--"));
  output_.print(F(" tx_authority=")); printAuthorization(snapshot.authorization);
  output_.println();
}

void DiagnosticConsole::printLocation(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[LOCATION]"));
  output_.print(F("locator=")); output_.print(snapshot.location.locator);
  output_.print(F(" source="));
  switch (snapshot.location.source) {
    case LocatorSource::FIXED: output_.println(F("FIXED")); break;
    case LocatorSource::GPS_LIVE: output_.println(F("GPS_LIVE")); break;
    case LocatorSource::GPS_LAST_KNOWN: output_.println(F("GPS_LAST_KNOWN")); break;
  }
}

void DiagnosticConsole::printRadio(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[RADIO]"));
  output_.print(F("tx=")); printTxState(snapshot.txState);
  output_.print(F(" mode="));
  output_.print(wsprFrequencyModeLabel(snapshot.frequencyMode));
  output_.print(F(" configured_frequency_hz="));
  output_.print(snapshot.configuredFrequencyHz);
  output_.print(F(" last_frame_frequency_hz="));
  if (snapshot.txTelemetry.lastFrame.valid) {
    output_.print(snapshot.txTelemetry.lastFrame.frequencyHz);
  } else {
    output_.print(F("--"));
  }
  output_.print(F(" next_slot="));
  if (snapshot.nextSlotValid()) printUtc(snapshot.nextSlotUtcMillis);
  else output_.print(F("--:--:--"));
  output_.print(F(" inhibit="));
  printTxInhibitReason(snapshot.txTelemetry.inhibitReason);
  output_.println();
  output_.print(F("frames_started="));
  output_.print(snapshot.txTelemetry.framesStarted);
  output_.print(F(" completed="));
  output_.print(snapshot.txTelemetry.framesCompleted);
  output_.print(F(" interrupted="));
  output_.println(snapshot.txTelemetry.framesInterrupted);
  printModule(DiagnosticModule::SI5351A, snapshot.diagnostics.modules[2],
              snapshot.uptimeMillis);
}

void DiagnosticConsole::printNetwork(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[NETWORK]"));
  output_.print(F("wifi=")); printWifiState(snapshot.wifiState);
  output_.print(F(" credentials="));
  output_.println(snapshot.wifiSaved() ? F("SAVED") : F("NOT_SAVED"));
  printModule(DiagnosticModule::WIFI, snapshot.diagnostics.modules[4],
              snapshot.uptimeMillis);
  printModule(DiagnosticModule::NTP, snapshot.diagnostics.modules[5],
              snapshot.uptimeMillis);
}

void DiagnosticConsole::printModules(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[MODULES]"));
  for (size_t index = 0; index < DIAGNOSTIC_MODULE_COUNT; ++index) {
    printModule(static_cast<DiagnosticModule>(index),
                snapshot.diagnostics.modules[index], snapshot.uptimeMillis);
  }
}

void DiagnosticConsole::printTxStatus(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[TX STATUS]"));
  output_.print(F("state=")); printTxState(snapshot.txState);
  output_.print(F(" inhibit="));
  printTxInhibitReason(snapshot.txTelemetry.inhibitReason);
  output_.println();
  output_.print(F("started=")); output_.print(snapshot.txTelemetry.framesStarted);
  output_.print(F(" completed=")); output_.print(snapshot.txTelemetry.framesCompleted);
  output_.print(F(" interrupted="));
  output_.println(snapshot.txTelemetry.framesInterrupted);
  output_.println(F("rf_result=FIRMWARE_ONLY_NOT_MEASURED"));
}

void DiagnosticConsole::printTxLast(const ConsoleStatusSnapshot &snapshot) {
  output_.println(F("[TX LAST]"));
  const TxFrameSnapshot &frame = snapshot.txTelemetry.lastFrame;
  if (!frame.valid) {
    output_.println(F("result=NONE"));
    return;
  }
  output_.print(F("slot=")); printUtc(frame.slotUtcMillis);
  output_.print(F(" source=")); printTxTimeSource(frame.timeSource);
  output_.print(F(" locator=")); output_.print(frame.locator);
  output_.print(F(" frequency_hz=")); output_.println(frame.frequencyHz);
  output_.print(F("result="));
  switch (frame.result) {
    case TxFrameResult::NONE: output_.print(F("NONE")); break;
    case TxFrameResult::COMPLETED_BY_FIRMWARE:
      output_.print(F("COMPLETED_BY_FIRMWARE")); break;
    case TxFrameResult::INTERRUPTED_RF_ERROR:
      output_.print(F("INTERRUPTED_RF_ERROR")); break;
    case TxFrameResult::INTERRUPTED_MANUAL_TX_OFF:
      output_.print(F("INTERRUPTED_MANUAL_TX_OFF")); break;
  }
  output_.print(F(" reason=")); printTxInhibitReason(frame.endReason);
  output_.print(F(" rf_verified="));
  output_.println(frame.rfVerified ? F("YES") : F("NO"));
}

void DiagnosticConsole::printModule(DiagnosticModule module,
                                    const DiagnosticRecord &record,
                                    uint32_t nowMillis) {
  output_.print(F("module="));
  switch (module) {
    case DiagnosticModule::GPS: output_.print(F("GPS")); break;
    case DiagnosticModule::PPS: output_.print(F("PPS")); break;
    case DiagnosticModule::SI5351A: output_.print(F("SI5351A")); break;
    case DiagnosticModule::OLED: output_.print(F("OLED")); break;
    case DiagnosticModule::WIFI: output_.print(F("WIFI")); break;
    case DiagnosticModule::NTP: output_.print(F("NTP")); break;
  }
  output_.print(F(" state=")); printState(record.state);
  output_.print(F(" reason=")); printReason(record.reason);
  output_.print(F(" age_ms="));
  output_.print(static_cast<uint32_t>(nowMillis - record.lastChangeMillis));
  output_.print(F(" changes=")); output_.print(record.counters.stateChanges);
  output_.print(F(" ok=")); output_.print(record.counters.successes);
  output_.print(F(" err=")); output_.println(record.counters.failures);
}

void DiagnosticConsole::printState(DiagnosticState state) {
  switch (state) {
    case DiagnosticState::STATE_UNKNOWN: output_.print(F("UNKNOWN")); break;
    case DiagnosticState::STATE_DISABLED: output_.print(F("DISABLED")); break;
    case DiagnosticState::STATE_NOT_DETECTED: output_.print(F("NOT_DETECTED")); break;
    case DiagnosticState::STATE_INITIALIZING: output_.print(F("INITIALIZING")); break;
    case DiagnosticState::STATE_OPERATIONAL: output_.print(F("OPERATIONAL")); break;
    case DiagnosticState::STATE_DEGRADED: output_.print(F("DEGRADED")); break;
    case DiagnosticState::STATE_ERROR: output_.print(F("ERROR")); break;
  }
}

void DiagnosticConsole::printReason(DiagnosticReason reason) {
  switch (reason) {
    case DiagnosticReason::NONE: output_.print(F("NONE")); break;
    case DiagnosticReason::NOT_EVALUATED: output_.print(F("NOT_EVALUATED")); break;
    case DiagnosticReason::DISABLED_BY_CONFIGURATION: output_.print(F("DISABLED_BY_CONFIG")); break;
    case DiagnosticReason::DEVICE_NOT_FOUND: output_.print(F("DEVICE_NOT_FOUND")); break;
    case DiagnosticReason::INITIALIZATION_FAILED: output_.print(F("INIT_FAILED")); break;
    case DiagnosticReason::COMMUNICATION_FAILURE: output_.print(F("COMM_FAILURE")); break;
    case DiagnosticReason::TIMEOUT: output_.print(F("TIMEOUT")); break;
    case DiagnosticReason::INVALID_DATA: output_.print(F("INVALID_DATA")); break;
    case DiagnosticReason::SERVICE_UNAVAILABLE: output_.print(F("SERVICE_UNAVAILABLE")); break;
    case DiagnosticReason::WAITING_FOR_DATA: output_.print(F("WAITING_FOR_DATA")); break;
    case DiagnosticReason::NOT_IMPLEMENTED: output_.print(F("NOT_IMPLEMENTED")); break;
    case DiagnosticReason::RF_OUTPUT_UNVERIFIED: output_.print(F("RF_UNVERIFIED")); break;
    case DiagnosticReason::INTERNAL_ERROR: output_.print(F("INTERNAL_ERROR")); break;
  }
}

void DiagnosticConsole::printUtc(uint32_t millisOfDay) {
  const uint32_t seconds = millisOfDay / 1000UL;
  const uint8_t hour = seconds / 3600UL;
  const uint8_t minute = (seconds / 60UL) % 60UL;
  const uint8_t second = seconds % 60UL;
  if (hour < 10) output_.print('0');
  output_.print(hour); output_.print(':');
  if (minute < 10) output_.print('0');
  output_.print(minute); output_.print(':');
  if (second < 10) output_.print('0');
  output_.print(second);
}

void DiagnosticConsole::printTimeSource(TimeSource source) {
  switch (source) {
    case TimeSource::NONE: output_.print(F("NONE")); break;
    case TimeSource::GPS: output_.print(F("GPS")); break;
    case TimeSource::NTP: output_.print(F("NTP")); break;
  }
}

void DiagnosticConsole::printTimeQuality(TimeQuality quality) {
  switch (quality) {
    case TimeQuality::UNKNOWN: output_.print(F("UNKNOWN")); break;
    case TimeQuality::COARSE: output_.print(F("COARSE")); break;
    case TimeQuality::SYNCHRONIZED: output_.print(F("SYNCHRONIZED")); break;
    case TimeQuality::PRECISE: output_.print(F("PRECISE")); break;
  }
}

void DiagnosticConsole::printAuthorization(WsprTimeAuthorization authorization) {
  switch (authorization) {
    case WsprTimeAuthorization::NONE: output_.print(F("INHIBITED")); break;
    case WsprTimeAuthorization::GPS_PRECISE: output_.print(F("GPS_PRECISE")); break;
    case WsprTimeAuthorization::NTP_EXPERIMENTAL: output_.print(F("NTP_EXPERIMENTAL")); break;
  }
}

void DiagnosticConsole::printTxInhibitReason(TxInhibitReason reason) {
  switch (reason) {
    case TxInhibitReason::NONE: output_.print(F("NONE")); break;
    case TxInhibitReason::RF_UNAVAILABLE: output_.print(F("RF_UNAVAILABLE")); break;
    case TxInhibitReason::TIME_UNAVAILABLE: output_.print(F("TIME_UNAVAILABLE")); break;
    case TxInhibitReason::TIME_UNQUALIFIED: output_.print(F("TIME_UNQUALIFIED")); break;
    case TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED:
      output_.print(F("OPERATING_IDENTITY_UNCONFIGURED")); break;
    case TxInhibitReason::MANUAL_TX_OFF:
      output_.print(F("MANUAL_TX_OFF")); break;
    case TxInhibitReason::SLOT_MISSED: output_.print(F("SLOT_MISSED")); break;
    case TxInhibitReason::RF_RUNTIME_ERROR: output_.print(F("RF_RUNTIME_ERROR")); break;
  }
}

void DiagnosticConsole::printTxTimeSource(TxTimeSource source) {
  switch (source) {
    case TxTimeSource::NONE: output_.print(F("NONE")); break;
    case TxTimeSource::GPS: output_.print(F("GPS")); break;
    case TxTimeSource::NTP: output_.print(F("NTP_EXPERIMENTAL")); break;
  }
}

void DiagnosticConsole::printTxState(ConsoleTxState state) {
  switch (state) {
    case ConsoleTxState::RF_INHIBITED: output_.print(F("RF_INHIBITED")); break;
    case ConsoleTxState::WAITING_FOR_TIME: output_.print(F("WAIT_TIME")); break;
    case ConsoleTxState::IDLE: output_.print(F("IDLE")); break;
    case ConsoleTxState::PREPARING: output_.print(F("PREPARING")); break;
    case ConsoleTxState::TRANSMITTING: output_.print(F("TRANSMITTING")); break;
  }
}

void DiagnosticConsole::printWifiState(WifiConnectionState state) {
  switch (state) {
    case WifiConnectionState::STATE_DISABLED: output_.print(F("DISABLED")); break;
    case WifiConnectionState::STATE_DISCONNECTED: output_.print(F("DISCONNECTED")); break;
    case WifiConnectionState::STATE_CONNECTING: output_.print(F("CONNECTING")); break;
    case WifiConnectionState::STATE_CONNECTED: output_.print(F("CONNECTED")); break;
    case WifiConnectionState::STATE_FAILED: output_.print(F("FAILED")); break;
    case WifiConnectionState::STATE_AP_OPERATIONAL:
      output_.print(F("AP_OPERATIONAL")); break;
  }
}
