#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <JTEncode.h>
#include <Wire.h>
#include <si5351.h>

#include "ApplicationStatusJson.h"
#include "BeaconConfig.h"
#include "BoardConfig.h"
#include "BootModeController.h"
#include "ApSetupMarker.h"
#include "ApSetupBootDecision.h"
#include "ApSetupRequestService.h"
#include "BootConfigurationDiagnostics.h"
#include "Button.h"
#include "ConsoleWebCoordinator.h"
#include "DiagnosticConsole.h"
#include "DiagnosticRegistry.h"
#include "DisplayController.h"
#include "TxDisplayCoordinator.h"
#include "TxEligibility.h"
#include "GpsReceiver.h"
#include "GpsTimeAdapter.h"
#include "HardwareCheckController.h"
#include "BoundedHttpServer.h"
#include "HttpServiceGate.h"
#include "LocatorService.h"
#include "ManualTxController.h"
#include "OledDisplayPolicy.h"
#include "FactoryResetCore.h"
#include "FactoryResetService.h"
#include "FactoryResetJson.h"
#include "FirmwareIdentity.h"
#include "ConfigurationManager.h"
#include "ConfigurationJson.h"
#include "ConfigurationService.h"
#include "ConfigurationMutationJson.h"
#include "ConfigurationSaveJson.h"
#include "ConfigurationSessionController.h"
#include "ConfigurationSessionJson.h"
#include "PlatformConfigStorage.h"
#include "PlatformEntropy.h"
#include "ProvisioningSecurity.h"
#include "ProvisioningOperationalJson.h"
#include "ProvisioningScanJson.h"
#include "ProvisioningWifiJson.h"
#include "SntpProvider.h"
#include "NtpSettings.h"
#include "SerialConfigConsole.h"
#include "Si5351RfDriver.h"
#include "StatusLedController.h"
#include "StartupController.h"
#include "TimeService.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include "TimeAuthorityLedController.h"
#endif
#include "WifiManager.h"
#include "WifiRestartController.h"
#include "WifiSettings.h"
#include "WsprTransmitter.h"
#include "WsprConfigurationRuntime.h"

#if defined(WSPR_GPS_SOFTWARE_SERIAL)
  #include <SoftwareSerial.h>
  SoftwareSerial gpsSerial(PIN_GPS_RX, PIN_GPS_TX);
#elif defined(WSPR_GPS_SERIAL2)
  HardwareSerial gpsSerial(2);
#endif

namespace {
volatile uint8_t ppsPendingCount = 0;
volatile uint32_t ppsTimestampMillis = 0;

void WSPR_ISR_ATTR onGpsPps() {
  ppsTimestampMillis = millis();
  if (ppsPendingCount != UINT8_MAX) ++ppsPendingCount;
}

void printBootConfigurationWarning(BootConfigurationWarning warning) {
  switch (warning) {
    case BootConfigurationWarning::WSPR_SLOT_REJECTED_RECOVERED_SAVED:
      Serial.println(F("WARN WSPR_CONFIG_SLOT_REJECTED_RECOVERED_SAVED"));
      break;
    case BootConfigurationWarning::WIFI_SLOT_REJECTED_RECOVERED_SAVED:
      Serial.println(F("WARN WIFI_CONFIG_SLOT_REJECTED_RECOVERED_SAVED"));
      break;
    case BootConfigurationWarning::WSPR_NO_VALID_RECORD_USING_DEFAULTS:
      Serial.println(F("WARN WSPR_CONFIG_NO_VALID_RECORD_USING_DEFAULTS"));
      break;
    case BootConfigurationWarning::WIFI_NO_VALID_RECORD_USING_DEFAULTS:
      Serial.println(F("WARN WIFI_CONFIG_NO_VALID_RECORD_USING_DEFAULTS"));
      break;
    case BootConfigurationWarning::NONE:
      break;
  }
}

BeaconConfig config;
Si5351 synthesizer;
Si5351RfDriver rfDriver(synthesizer);
RfSafetyController rfSafety(rfDriver, config.si5351ReferenceHz,
                            config.si5351Correction);
JTEncode encoder;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
DisplayController screen(oled);
void applyPowerSaveSetting(bool enabled) { screen.setPowerSave(enabled); }
WsprTransmitter transmitter(config, rfSafety, encoder);
ManualTxController manualTx(transmitter);
ConfigurationSessionController configurationSessions;
Button bandButton(PIN_BUTTON);
WifiManager wifi;
#if WSPR_HAS_WIFI
PlatformConfigStorage configStorage;
ConfigurationManager configurationManager(configStorage);
FactoryResetCore factoryReset(configStorage);
ApSetupMarker apSetupMarker(configStorage);
ApSetupMarkerState apSetupBootState = ApSetupMarkerState::ABSENT;
WsprConfigurationRuntime configurationRuntime(transmitter,
                                               applyPowerSaveSetting);
ConfigurationService configurationService(configurationManager,
                                          configurationRuntime);
FactoryResetService factoryResetService(factoryReset, configurationManager,
                                        configurationSessions, manualTx,
                                        transmitter);
BoundedHttpServer httpServer;
WifiRestartController wifiRestart;
BootModeController bootMode;
char provisioningSsid[33] = {};
char provisioningPassword[ProvisioningSecurity::PASSWORD_DIGITS + 1U] = {};
bool configStorageReady = false;
HttpServiceGate::Decision currentHttpDecision;
#endif
DiagnosticConsole diagnosticConsole(Serial);
SerialConfigConsole serialConfigConsole(Serial);
StatusLedController statusLed;
#if defined(ARDUINO_ARCH_ESP8266)
TimeAuthorityLedController timeAuthorityLed;
#endif
StartupController startup;
HardwareCheckController hardwareCheck;
TxDisplayCoordinator txDisplayCoordinator;
bool oledInitialized = false;
TimeService timeService(
    TimeServiceConfig(5000UL, NTP_REFERENCE_TIMEOUT_MS));
GpsTimeAdapter gpsTimeAdapter;
LocatorService locatorService;
#if WSPR_HAS_WIFI
#if defined(ARDUINO_ARCH_ESP32)
  SntpProvider sntpProvider(
      NtpProviderConfig(NTP_REQUEST_TIMEOUT_MS, NTP_REFERENCE_TIMEOUT_MS,
                        ESP32_NTP_RECOVERY_INITIAL_MS,
                        ESP32_NTP_RECOVERY_MAX_MS));
#else
  SntpProvider sntpProvider(
      NtpProviderConfig(NTP_REQUEST_TIMEOUT_MS, NTP_REFERENCE_TIMEOUT_MS));
#endif
  bool ntpProviderEnabled = false;
#endif

Stream &gpsStream() {
#if defined(WSPR_GPS_SERIAL1)
  return Serial1;
#else
  return gpsSerial;
#endif
}

GpsReceiver gps(gpsStream());

void beginGps() {
#if defined(WSPR_GPS_SERIAL1)
  Serial1.begin(9600);
#elif defined(WSPR_GPS_SERIAL2)
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
#else
  gpsSerial.begin(9600);
#endif
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void updateRfDiagnostics(uint32_t nowMillis) {
  DiagnosticState state = DiagnosticState::STATE_ERROR;
  DiagnosticReason reason = DiagnosticReason::COMMUNICATION_FAILURE;
  switch (rfSafety.fault()) {
    case RfFault::NONE:
      state = DiagnosticState::STATE_DEGRADED;
      reason = DiagnosticReason::RF_OUTPUT_UNVERIFIED;
      break;
    case RfFault::NOT_DETECTED:
      state = DiagnosticState::STATE_NOT_DETECTED;
      reason = DiagnosticReason::DEVICE_NOT_FOUND;
      break;
    case RfFault::INITIALIZATION_FAILED:
      reason = DiagnosticReason::INITIALIZATION_FAILED;
      break;
    case RfFault::FREQUENCY_CONFIGURATION_FAILED:
    case RfFault::CRITICAL_RUNTIME_ERROR:
      reason = DiagnosticReason::COMMUNICATION_FAILURE;
      break;
  }
  if (diagnostics.setState(DiagnosticModule::SI5351A, state, reason,
                           nowMillis)) {
    if (rfSafety.operational()) {
      diagnostics.recordSuccess(DiagnosticModule::SI5351A);
    } else {
      diagnostics.recordFailure(DiagnosticModule::SI5351A);
    }
  }
}

#if WSPR_HAS_WIFI
ApplicationTxState applicationTxState(TransmitterState state) {
  switch (state) {
    case TransmitterState::RF_INHIBITED:
      return ApplicationTxState::RF_INHIBITED;
    case TransmitterState::WAITING_FOR_TIME:
      return ApplicationTxState::WAITING_FOR_TIME;
    case TransmitterState::IDLE: return ApplicationTxState::IDLE;
    case TransmitterState::PREPARING: return ApplicationTxState::PREPARING;
    case TransmitterState::TRANSMITTING:
      return ApplicationTxState::TRANSMITTING;
  }
  return ApplicationTxState::WAITING_FOR_TIME;
}

void copyStatusText(char *destination, size_t capacity, const char *source) {
  if (destination == nullptr || capacity == 0) return;
  if (source == nullptr) source = "";
  strncpy(destination, source, capacity - 1U);
  destination[capacity - 1U] = '\0';
}

int32_t coordinateMicrodegrees(float degrees) {
  const double scaled = static_cast<double>(degrees) * 1000000.0;
  return static_cast<int32_t>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
}

bool effectiveTxEligibility(uint32_t nowMillis) {
  const WsprTimeAuthorization authorization =
      transmitter.timeAuthorization(timeService.snapshot(nowMillis));
  const ConfigurationSessionSnapshot session =
      configurationSessions.snapshot(nowMillis);
  TxEligibilityInputs inputs;
  inputs.manualPermission = manualTx.permitted();
  inputs.operationalMode = bootMode.rfOperationAllowed();
  inputs.identityConfigured = transmitter.operatingIdentityState() ==
                              OperatingIdentityState::CONFIGURED;
  inputs.rfReady = rfSafety.operational();
  inputs.configurationActive =
      session.mode == FirmwareOperatingMode::CONFIGURATION;
  inputs.resumeGuardActive = session.resumeGuardActive;
  inputs.timeQualified = bootMode.timeAuthorizationAllowed(authorization);
  return TxEligibility::evaluate(inputs);
}

bool provideApplicationStatus(void *, uint32_t nowMillis, char *destination,
                              size_t capacity, size_t &length) {
  const TransmitterState currentState = transmitter.state();
  if (currentState == TransmitterState::PREPARING ||
      currentState == TransmitterState::TRANSMITTING) {
    length = 0;
    return false;
  }

  ApplicationStatusSnapshot snapshot;
  snapshot.uptimeMillis = nowMillis;
  snapshot.time = timeService.snapshot(nowMillis);
  snapshot.authorization = transmitter.timeAuthorization(snapshot.time);
  snapshot.locatorSource = locatorService.snapshot().source;
  snapshot.frequencyMode = transmitter.frequencyMode();
  snapshot.txState = applicationTxState(currentState);
  snapshot.rfReady = rfSafety.operational();
  snapshot.manualTxPermitted = manualTx.permitted();
  snapshot.effectiveTxEligible = effectiveTxEligibility(nowMillis);
  snapshot.configuredFrequencyHz = transmitter.configuredFrequencyHz();
  snapshot.telemetry = transmitter.telemetry();
  snapshot.wifiState = bootMode.apOperational()
      ? WifiConnectionState::STATE_AP_OPERATIONAL : wifi.state();
  snapshot.freeHeapBytes = ESP.getFreeHeap();
  snapshot.freeHeapValid = true;
  snapshot.operatingMode = configurationSessions.snapshot(nowMillis);

  const LocatorSnapshot location = locatorService.snapshot();
  const TxPreparedSnapshot &prepared = transmitter.preparedSnapshot();
  const WsprMessageSnapshot message =
      prepared.valid ? prepared.message : transmitter.messageSnapshot(location);
  copyStatusText(snapshot.callsign, sizeof(snapshot.callsign), message.callsign);
  copyStatusText(snapshot.locator, sizeof(snapshot.locator), message.locator);
  snapshot.powerDbm = message.powerDbm;
  const WsprBandId band = prepared.valid ? prepared.band : transmitter.bandId();
  const BandPlanEntry *bandEntry = BandPlan::find(band);
  copyStatusText(snapshot.band, sizeof(snapshot.band),
                 bandEntry == nullptr ? "--" : bandEntry->name);
  if (prepared.valid) {
    snapshot.frequencyMode = prepared.frequencyMode;
    snapshot.selectedFrequencyHz = prepared.frequencyHz;
    snapshot.selectedFrequencyValid = true;
  }

  snapshot.nextSlotValid = WsprScheduler::nextSlotUtcMillis(
      snapshot.time, config.transmitIntervalMinutes,
      snapshot.nextSlotUtcMillis);
  uint32_t remainingMillis = 0;
  if (snapshot.nextSlotValid && WsprScheduler::millisUntilNextSlot(
          snapshot.time, config.transmitIntervalMinutes, remainingMillis)) {
    snapshot.remainingMillis = remainingMillis;
  } else {
    snapshot.nextSlotValid = false;
  }

  char ipv4[16];
  wifi.localIpv4(ipv4);
  if (wifi.connected() || bootMode.apOperational()) {
    copyStatusText(snapshot.ipv4, sizeof(snapshot.ipv4), ipv4);
  }

  const GpsData &gpsData = gps.data();
  snapshot.gps.nmeaAvailable = gpsData.nmeaAvailable;
  snapshot.gps.fixValid = gpsData.positionValid;
  snapshot.gps.satellites = gpsData.satellites;
  snapshot.gps.latitudeMicrodegrees = coordinateMicrodegrees(gpsData.latitude);
  snapshot.gps.longitudeMicrodegrees = coordinateMicrodegrees(gpsData.longitude);
  snapshot.gps.ppsPresent = gpsData.ppsAvailable;
  snapshot.gps.ppsAssociated = gpsData.ppsAssociated;
  diagnostics.snapshot(snapshot.diagnostics);
  copyStatusText(snapshot.board, sizeof(snapshot.board), WSPR_BOARD_NAME);
  return ApplicationStatusJson::serialize(snapshot, destination, capacity,
                                          length);
}

bool provideConfigurationStatus(void *, uint32_t, char *destination,
                                size_t capacity, size_t &length) {
  const TransmitterState currentState = transmitter.state();
  if (currentState == TransmitterState::PREPARING ||
      currentState == TransmitterState::TRANSMITTING) {
    length = 0;
    return false;
  }
  return ConfigurationJson::serialize(configurationService.snapshot(),
                                      destination, capacity, length);
}

bool provideConfigurationSessionStatus(void *, uint32_t nowMillis,
                                       char *destination, size_t capacity,
                                       size_t &length) {
  configurationSessions.expireWeb(nowMillis, transmitter);
  return ConfigurationSessionJson::serialize(
      configurationSessions.snapshot(nowMillis), destination, capacity, length);
}

bool provideProvisioningScanStatus(void *, uint32_t nowMillis,
                                   char *destination, size_t capacity,
                                   size_t &length) {
  if (!bootMode.provisioning()) {
    length = 0;
    return false;
  }
  return ProvisioningScanJson::serialize(
      wifi.scanSnapshot(), nowMillis, destination, capacity, length);
}

void provideConfigurationMutation(
    void *, HttpTransportCore::Mutation mutation, const char *requestBody,
    size_t requestLength, const char *sessionHeader, uint32_t nowMillis,
    uint16_t &statusCode, char *destination, size_t capacity, size_t &length) {
  length = 0;
  auto error = [&](uint16_t code, const char *name) {
    statusCode = code;
    const int written = snprintf(destination, capacity,
                                 "{\"schema\":1,\"error\":\"%s\"}\n", name);
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
  };
  configurationSessions.expireWeb(nowMillis, transmitter);
  if (!currentHttpDecision.allowed ||
      transmitter.state() == TransmitterState::PREPARING ||
      transmitter.state() == TransmitterState::TRANSMITTING ||
      transmitter.rfOutputEnabled()) {
    error(503, "rf_unsafe");
    return;
  }
  if (wifiRestart.pending() &&
      mutation != HttpTransportCore::Mutation::SESSION_HEARTBEAT &&
      mutation != HttpTransportCore::Mutation::SESSION_EXIT &&
      mutation != HttpTransportCore::Mutation::MANUAL_TX_OFF) {
    error(409, "restart_pending");
    return;
  }
  if (mutation == HttpTransportCore::Mutation::MANUAL_TX_OFF ||
      mutation == HttpTransportCore::Mutation::MANUAL_TX_ON) {
    const bool turnOn = mutation == HttpTransportCore::Mutation::MANUAL_TX_ON;
    const char *expected = turnOn ? "{\"confirm\":\"tx_on\"}" : "{}";
    if (requestLength != strlen(expected) ||
        memcmp(requestBody, expected, requestLength) != 0) {
      error(400, "invalid_request");
      return;
    }
    if (turnOn && !manualTx.turnOn(bootMode.rfOperationAllowed())) {
      error(403, "non_operational_mode");
      return;
    }
    if (!turnOn) manualTx.turnOff();
    statusCode = 200;
    const int written = snprintf(
        destination, capacity,
        "{\"schema\":1,\"manual_permission\":%s,\"effective_eligible\":%s}\n",
        manualTx.permitted() ? "true" : "false",
        effectiveTxEligibility(nowMillis) ? "true" : "false");
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
    return;
  }
  if (mutation == HttpTransportCore::Mutation::FACTORY_RESET) {
    FactoryResetRequest request;
    const FactoryResetParseResult parsed =
        FactoryResetJson::parse(requestBody, requestLength, request);
    if (parsed != FactoryResetParseResult::OK) {
      error(parsed == FactoryResetParseResult::OUT_OF_RANGE ? 422 : 400,
            "invalid_request");
      return;
    }
    const FactoryResetServiceResult result = factoryResetService.execute(
        FactoryResetOwner::WEB, sessionHeader, request.expectedRevision,
        nowMillis);
    const char *label = "reset_failed";
    uint16_t code = 500;
    switch (result) {
      case FactoryResetServiceResult::COMPLETED: label = "completed"; code = 200; break;
      case FactoryResetServiceResult::ALREADY_COMPLETED: label = "already_completed"; code = 409; break;
      case FactoryResetServiceResult::SESSION_REQUIRED: label = "invalid_session"; code = 403; break;
      case FactoryResetServiceResult::OWNER_CONFLICT: label = "owner_conflict"; code = 409; break;
      case FactoryResetServiceResult::STALE_REVISION: label = "stale_mode_revision"; code = 409; break;
      case FactoryResetServiceResult::OPERATION_BUSY: label = "operation_busy"; code = 409; break;
      case FactoryResetServiceResult::STORAGE_FAILED: label = "storage_failed"; break;
      case FactoryResetServiceResult::VERIFICATION_FAILED: label = "verification_failed"; break;
      case FactoryResetServiceResult::RUNTIME_FAILED: label = "runtime_failed"; break;
    }
    if (result == FactoryResetServiceResult::COMPLETED) {
      httpServer.beginSecurity();
    }
    statusCode = code;
    const int written = snprintf(destination, capacity,
        "{\"schema\":1,\"result\":\"%s\",\"restart_required\":true,\"tx_inhibited\":true}\n",
        label);
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
    return;
  }
  if (factoryResetService.resetCompletedThisBoot()) {
    error(409, "factory_reset_completed_restart_required");
    return;
  }
  if (mutation == HttpTransportCore::Mutation::ENTER_AP_SETUP) {
    const ApSetupRequestResult result = ApSetupRequestService::execute(
        requestBody, requestLength, sessionHeader, nowMillis,
        configurationService.revision(),
        bootMode.normal() || bootMode.apOperational(),
        factoryReset.factoryStateRequired(), configurationSessions,
        transmitter, apSetupMarker, wifiRestart);
    if (result != ApSetupRequestResult::ACCEPTED) {
      switch (result) {
        case ApSetupRequestResult::OPERATIONAL_MODE_REQUIRED:
          error(403, "operational_mode_required"); break;
        case ApSetupRequestResult::FACTORY_RESET_PENDING:
          error(409, "factory_reset_pending"); break;
        case ApSetupRequestResult::RESTART_PENDING:
          error(409, "restart_pending"); break;
        case ApSetupRequestResult::INVALID_SESSION:
          error(403, "invalid_session"); break;
        case ApSetupRequestResult::INVALID_REQUEST:
          error(400, "invalid_request"); break;
        case ApSetupRequestResult::STALE_REVISION:
          error(409, "stale_revision"); break;
        case ApSetupRequestResult::RF_UNSAFE:
          error(503, "rf_unsafe"); break;
        case ApSetupRequestResult::OPERATION_BUSY:
          error(409, "operation_busy"); break;
        case ApSetupRequestResult::ALREADY_PENDING:
          error(409, "setup_already_pending"); break;
        case ApSetupRequestResult::MARKER_WRITE_FAILED:
          error(500, "marker_storage_failed"); break;
        case ApSetupRequestResult::MARKER_VERIFICATION_FAILED:
          error(500, "marker_verification_failed"); break;
        case ApSetupRequestResult::ACCEPTED: break;
      }
      return;
    }
    statusCode = 202;
    const int written = snprintf(destination, capacity,
        "{\"schema\":1,\"result\":\"ap_setup_pending\",\"restart_pending\":true}\n");
    length = written > 0 && static_cast<size_t>(written) < capacity
        ? static_cast<size_t>(written) : 0;
    return;
  }
  if (mutation == HttpTransportCore::Mutation::ENTER_AP_OPERATIONAL) {
    if (!bootMode.provisioning()) {
      error(403, "provisioning_required");
      return;
    }
    const WifiScanSnapshot &scan = wifi.scanSnapshot();
    if (scan.state == WifiScanState::SCANNING || scan.driverBusy) {
      error(409, "scan_busy");
      return;
    }
    if (configurationSessions.operationActive()) {
      error(409, "operation_busy");
      return;
    }
    if (!ProvisioningOperationalJson::valid(requestBody, requestLength)) {
      error(400, "invalid_request");
      return;
    }
    const ApOperationalRequestResult transition =
        bootMode.requestApOperational();
    if (transition == ApOperationalRequestResult::REJECTED) {
      error(409, "transition_rejected");
      return;
    }
    statusCode = 202;
    const char *label = transition == ApOperationalRequestResult::ACCEPTED
        ? "transition_pending" : "already_pending";
    const int written = snprintf(destination, capacity,
        "{\"schema\":1,\"result\":\"%s\",\"tx_disabled\":true}\n",
        label);
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
    return;
  }
  if (mutation == HttpTransportCore::Mutation::START_SCAN) {
    if (!bootMode.provisioning()) {
      error(403, "provisioning_required");
      return;
    }
    size_t cursor = 0;
    while (cursor < requestLength &&
           (requestBody[cursor] == ' ' || requestBody[cursor] == '\t' ||
            requestBody[cursor] == '\r' || requestBody[cursor] == '\n')) ++cursor;
    const bool emptyObject = cursor < requestLength && requestBody[cursor++] == '{';
    while (cursor < requestLength &&
           (requestBody[cursor] == ' ' || requestBody[cursor] == '\t' ||
            requestBody[cursor] == '\r' || requestBody[cursor] == '\n')) ++cursor;
    const bool closed = cursor < requestLength && requestBody[cursor++] == '}';
    while (cursor < requestLength &&
           (requestBody[cursor] == ' ' || requestBody[cursor] == '\t' ||
            requestBody[cursor] == '\r' || requestBody[cursor] == '\n')) ++cursor;
    if (!emptyObject || !closed || cursor != requestLength) {
      error(400, "invalid_request");
      return;
    }
    const WifiScanStartResult result =
        wifi.startProvisioningScan(nowMillis);
    if (result == WifiScanStartResult::START_FAILED) {
      error(503, "scan_start_failed");
      return;
    }
    if (result == WifiScanStartResult::DRIVER_BUSY) {
      error(409, "scan_driver_busy");
      return;
    }
    statusCode = 202;
    const char *label = result == WifiScanStartResult::ALREADY_SCANNING
        ? "already_scanning" : "started";
    const int written = snprintf(destination, capacity,
        "{\"schema\":1,\"result\":\"%s\",\"generation\":%lu}\n",
        label, static_cast<unsigned long>(wifi.scanSnapshot().generation));
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
    return;
  }
  if (mutation == HttpTransportCore::Mutation::SAVE_WIFI) {
    if (!bootMode.provisioning()) {
      error(403, "provisioning_required");
      return;
    }
    WifiCredentials credentials;
    const ProvisioningWifiParseResult parsed =
        ProvisioningWifiJson::parse(requestBody, requestLength, credentials);
    if (parsed != ProvisioningWifiParseResult::OK) {
      credentials.clearPassword();
      error(parsed == ProvisioningWifiParseResult::OUT_OF_RANGE ? 422 : 400,
            "invalid_credentials");
      return;
    }
    const ProvisioningWifiSaveResult result =
        configurationService.saveProvisioningWifi(credentials, true);
    credentials.clearPassword();
    const char *label = "persistence_failed";
    uint16_t code = 500;
    if (result == ProvisioningWifiSaveResult::SAVED) {
      label = "saved"; code = 200;
    } else if (result == ProvisioningWifiSaveResult::UNCHANGED) {
      label = "unchanged"; code = 200;
    } else if (result == ProvisioningWifiSaveResult::INVALID) {
      label = "invalid_credentials"; code = 422;
    } else if (result == ProvisioningWifiSaveResult::NOT_PROVISIONING) {
      label = "provisioning_required"; code = 403;
    } else if (result == ProvisioningWifiSaveResult::VERIFICATION_FAILED) {
      label = "verification_failed";
    }
    statusCode = code;
    const int written = snprintf(destination, capacity,
        "{\"schema\":1,\"result\":\"%s\",\"restart_required\":true,\"tx_disabled\":true}\n",
        label);
    length = written > 0 && static_cast<size_t>(written) < capacity
                 ? static_cast<size_t>(written) : 0;
    return;
  }
  const bool sessionMutation =
      mutation == HttpTransportCore::Mutation::SESSION_ENTER ||
      mutation == HttpTransportCore::Mutation::SESSION_HEARTBEAT ||
      mutation == HttpTransportCore::Mutation::SESSION_EXIT;
  if (sessionMutation) {
    char owner[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH + 1U] = {};
    ConfigurationSessionResult sessionResult = ConfigurationSessionResult::INVALID_IDENTIFIER;
    if (mutation == HttpTransportCore::Mutation::SESSION_ENTER) {
      ConfigurationSessionEnterRequest request;
      const ConfigurationSessionParseResult parsed =
          ConfigurationSessionJson::parseEnter(requestBody, requestLength, request);
      if (parsed != ConfigurationSessionParseResult::OK) {
        error(parsed == ConfigurationSessionParseResult::OUT_OF_RANGE ? 422 : 400,
              "invalid_request");
        return;
      }
      if (request.expectedRevision != configurationSessions.revision()) {
        const ConfigurationSessionResult existingOwner =
            configurationSessions.authorizeWebMutation(
                request.sessionId, nowMillis, transmitter);
        if (existingOwner != ConfigurationSessionResult::OK) {
          error(409, "stale_mode_revision");
          return;
        }
      }
      sessionResult = configurationSessions.acquireWeb(
          request.sessionId, nowMillis, transmitter);
    } else {
      const ConfigurationSessionParseResult parsed =
          ConfigurationSessionJson::parseOwner(requestBody, requestLength, owner);
      if (parsed != ConfigurationSessionParseResult::OK) {
        error(parsed == ConfigurationSessionParseResult::OUT_OF_RANGE ? 422 : 400,
              "invalid_request");
        return;
      }
      sessionResult = mutation == HttpTransportCore::Mutation::SESSION_HEARTBEAT
          ? configurationSessions.renewWeb(owner, nowMillis, transmitter)
          : configurationSessions.releaseWeb(owner, transmitter);
    }
    if (sessionResult == ConfigurationSessionResult::RF_BUSY) {
      error(503, "rf_unsafe");
      return;
    }
    if (sessionResult == ConfigurationSessionResult::OWNER_MISMATCH) {
      error(403, "session_owner_mismatch");
      return;
    }
    if (sessionResult == ConfigurationSessionResult::NOT_ACTIVE) {
      error(409, "session_not_active");
      return;
    }
    if (sessionResult != ConfigurationSessionResult::OK &&
        sessionResult != ConfigurationSessionResult::UNCHANGED) {
      error(422, "invalid_session");
      return;
    }
    statusCode = 200;
    if (!ConfigurationSessionJson::serialize(
            configurationSessions.snapshot(nowMillis), destination, capacity,
            length)) {
      error(503, "response_unavailable");
    }
    return;
  }

  if (mutation == HttpTransportCore::Mutation::SAVE_WSPR) {
    const ConfigurationSessionResult authorization =
        configurationSessions.authorizeWebMutation(
            sessionHeader, nowMillis, transmitter);
    if (authorization != ConfigurationSessionResult::OK) {
      error(403, "invalid_session");
      return;
    }
    ConfigurationSaveRequest request;
    const ConfigurationMutationParseResult parsed =
        ConfigurationMutationJson::parseSave(requestBody, requestLength,
                                             request);
    if (parsed != ConfigurationMutationParseResult::OK) {
      error(parsed == ConfigurationMutationParseResult::OUT_OF_RANGE ? 422 : 400,
            "invalid_request");
      return;
    }
    WsprSaveSafetyContext safety = {
        configurationSessions, transmitter, ConfigurationSaveOwner::WEB,
        sessionHeader, nowMillis};
    const WsprSaveOutcome outcome =
        configurationService.saveWspr(request.expectedRevision, safety);
    uint16_t code = 500;
    switch (outcome.result) {
      case WsprSaveResult::SAVED_APPLIED:
        code = 200; break;
      case WsprSaveResult::UNCHANGED:
        code = 200; break;
      case WsprSaveResult::STALE_REVISION:
        code = 409; break;
      case WsprSaveResult::CONFIG_MODE_REQUIRED:
        code = 403; break;
      case WsprSaveResult::RF_UNSAFE:
        code = 503; break;
      case WsprSaveResult::INVALID_CONFIGURATION:
        code = 422; break;
      case WsprSaveResult::PERSISTENCE_FAILED:
        code = 500; break;
      case WsprSaveResult::VERIFICATION_FAILED:
        code = 500; break;
      case WsprSaveResult::PERSISTED_APPLY_FAILED:
        code = 500; break;
    }
    statusCode = code;
    if (!ConfigurationSaveJson::serialize(
            outcome, configurationService.revision(), destination, capacity,
            length)) {
      error(503, "response_unavailable");
    }
    return;
  }

  const ConfigurationSessionResult authorization =
      configurationSessions.authorizeWebMutation(sessionHeader, nowMillis,
                                                  transmitter);
  if (authorization != ConfigurationSessionResult::OK) {
    error(403, "web_session_required");
    return;
  }

  uint32_t expectedRevision = 0;
  ConfigurationOperationResult result = ConfigurationOperationResult::INVALID;
  if (mutation == HttpTransportCore::Mutation::STAGE_CANDIDATE) {
    ConfigurationCandidateRequest request;
    const ConfigurationMutationParseResult parsed =
        ConfigurationMutationJson::parseCandidate(requestBody, requestLength,
                                                  request);
    if (parsed != ConfigurationMutationParseResult::OK) {
      error(parsed == ConfigurationMutationParseResult::OUT_OF_RANGE ? 422 : 400,
            "invalid_request");
      return;
    }
    expectedRevision = request.expectedRevision;
    result = configurationService.stageWspr(request.settings, expectedRevision);
  } else {
    const ConfigurationMutationParseResult parsed =
        ConfigurationMutationJson::parseRevision(requestBody, requestLength,
                                                 expectedRevision);
    if (parsed != ConfigurationMutationParseResult::OK) {
      error(parsed == ConfigurationMutationParseResult::OUT_OF_RANGE ? 422 : 400,
            "invalid_request");
      return;
    }
    result = configurationService.discardWspr(expectedRevision);
  }
  if (result == ConfigurationOperationResult::STALE_REVISION) {
    error(409, "stale_revision");
    return;
  }
  if (result == ConfigurationOperationResult::RF_BUSY) {
    error(503, "rf_unsafe");
    return;
  }
  if (result == ConfigurationOperationResult::INVALID) {
    error(422, "invalid_configuration");
    return;
  }
  if (result != ConfigurationOperationResult::OK &&
      result != ConfigurationOperationResult::UNCHANGED) {
    error(409, "configuration_conflict");
    return;
  }
  statusCode = 200;
  if (!ConfigurationJson::serialize(configurationService.snapshot(), destination,
                                    capacity, length)) {
    error(503, "response_unavailable");
  }
}
#endif
}  // namespace

void setup() {
  Serial.begin(9600);
  diagnosticConsole.setConfigurationSessions(&configurationSessions);
#if defined(ARDUINO_ARCH_ESP8266)
  beginEsp8266StatusLeds();
#else
  pinMode(PIN_STATUS_LED, OUTPUT);
  setStatusLed(false);
#endif
  bandButton.begin();
  pinMode(PIN_GPS_PPS, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_GPS_PPS), onGpsPps, RISING);
  beginGps();
  diagnostics.setState(DiagnosticModule::GPS, DiagnosticState::STATE_INITIALIZING,
                       DiagnosticReason::WAITING_FOR_DATA, millis());
  diagnostics.setState(DiagnosticModule::PPS, DiagnosticState::STATE_INITIALIZING,
                       DiagnosticReason::WAITING_FOR_DATA, millis());
  transmitter.seedFrequencySelector(platformEntropySeed());

  Serial.println();
  char firmwareIdentity[96];
  if (FirmwareIdentity::format(firmwareIdentity, sizeof(firmwareIdentity))) {
    Serial.println(firmwareIdentity);
  }

#if WSPR_HAS_WIFI
  httpServer.beginSecurity();
  httpServer.setStatusSnapshotProvider(provideApplicationStatus, nullptr);
  httpServer.setConfigSnapshotProvider(provideConfigurationStatus, nullptr);
  httpServer.setSessionSnapshotProvider(provideConfigurationSessionStatus,
                                        nullptr);
  httpServer.setScanSnapshotProvider(provideProvisioningScanStatus, nullptr);
  httpServer.setMutationProvider(provideConfigurationMutation, nullptr);
  configStorageReady = configStorage.begin();
  const FactoryResetBootState factoryResetState = factoryReset.recover(manualTx);
  const bool completedFactoryReset =
      factoryResetState == FactoryResetBootState::COMPLETED ||
      factoryResetState == FactoryResetBootState::RECOVERED;
  configurationManager.begin(factoryReset.factoryStateRequired(),
                             completedFactoryReset);
  // Factory Reset owns the boot decision when its marker is active.
  if (factoryResetState == FactoryResetBootState::NORMAL) {
    apSetupBootState = apSetupMarker.read();
    if (apSetupBootState == ApSetupMarkerState::CORRUPT)
      Serial.println(F("ERR AP_SETUP_MARKER_CORRUPT AP_REQUIRED RF_DISABLED"));
    else if (apSetupBootState == ApSetupMarkerState::STORAGE_ERROR)
      Serial.println(F("ERR AP_SETUP_MARKER_READ_FAILED AP_REQUIRED RF_DISABLED"));
  }
  if (factoryResetState == FactoryResetBootState::RECOVERED) {
    Serial.println(F("WARN FACTORY_RESET_RECOVERED REBOOT_REQUIRED"));
  } else if (factoryResetState == FactoryResetBootState::COMPLETED) {
    Serial.println(F("INFO FACTORY_RESET_STATE REBOOT_REQUIRED"));
  } else if (factoryResetState == FactoryResetBootState::FAIL_CLOSED) {
    Serial.println(F("ERR FACTORY_RESET_METADATA_FAIL_CLOSED"));
  }
  serialConfigConsole.begin(&configurationManager, &configurationService,
                            &configurationSessions, &manualTx,
                            &factoryResetService);
  const ConfigurationStatus configurationStatus = configurationManager.status();
  printBootConfigurationWarning(BootConfigurationDiagnostics::classify(
      BootConfigurationDomain::WSPR,
      configurationStatus.wsprRecordRejected, configurationStatus.wsprOrigin,
      factoryResetState));
  printBootConfigurationWarning(BootConfigurationDiagnostics::classify(
      BootConfigurationDomain::WIFI,
      configurationStatus.wifiRecordRejected, configurationStatus.wifiOrigin,
      factoryResetState));
  const bool identityConfigured =
      configurationManager.activeIdentityState() ==
      OperatingIdentityState::CONFIGURED;
  const bool settingsApplied = identityConfigured
      ? transmitter.applySettings(configurationManager.activeWspr())
      : transmitter.setOperatingIdentityUnconfigured(
            configurationManager.activeWspr());
  if (!settingsApplied) {
    Serial.println(F("ERR WSPR_CONFIG_APPLY_FAILED"));
  }
#else
  serialConfigConsole.begin(nullptr, nullptr, &configurationSessions,
                            &manualTx);
#endif

  diagnostics.setState(DiagnosticModule::SI5351A, DiagnosticState::STATE_INITIALIZING,
                       DiagnosticReason::NONE, millis());
  transmitter.begin();
  updateRfDiagnostics(millis());

  diagnostics.setState(DiagnosticModule::OLED, DiagnosticState::STATE_INITIALIZING,
                       DiagnosticReason::NONE, millis());
  oledInitialized = screen.begin();
#if WSPR_HAS_WIFI
  screen.beginPowerSave(configurationManager.activeWspr().powerSave);
#else
  screen.beginPowerSave(false);
#endif
  const uint32_t startupStartedMillis = millis();
  startup.begin(startupStartedMillis);
  statusLed.begin(startupStartedMillis);
#if defined(ARDUINO_ARCH_ESP8266)
  timeAuthorityLed.begin(startupStartedMillis);
#endif
  StatusLedInputs startupLedInputs;
  startupLedInputs.startupActive = true;
  const bool startupStatusLed =
      statusLed.update(startupStartedMillis, startupLedInputs).enabled;
  setStatusLed(screen.powerSaveEnabled() ? false : startupStatusLed);
  const bool oledDetected = i2cDevicePresent(0x3C);
  if (oledDetected && oledInitialized) {
    diagnostics.recordSuccess(DiagnosticModule::OLED);
    diagnostics.setState(DiagnosticModule::OLED, DiagnosticState::STATE_OPERATIONAL,
                         DiagnosticReason::NONE, millis());
  } else if (!oledDetected) {
    diagnostics.recordFailure(DiagnosticModule::OLED);
    diagnostics.setState(DiagnosticModule::OLED, DiagnosticState::STATE_NOT_DETECTED,
                         DiagnosticReason::DEVICE_NOT_FOUND, millis());
  } else {
    diagnostics.recordFailure(DiagnosticModule::OLED);
    diagnostics.setState(DiagnosticModule::OLED, DiagnosticState::STATE_ERROR,
                         DiagnosticReason::INITIALIZATION_FAILED, millis());
  }
  if (!oledInitialized) Serial.println(F("SSD1306 not found"));

#if WSPR_HAS_WIFI
  const WifiCredentials &savedWifi = configurationManager.activeWifi();
  const bool autoConnect = ApSetupBootDecision::trySta(
      configStorageReady,
      configurationManager.status().wifiOrigin == ConfigOrigin::SAVED &&
          savedWifi.complete(),
      factoryReset.bootState(), apSetupBootState);
  config.wifiEnabled = autoConnect;
  wifi.begin(autoConnect, savedWifi.ssid, savedWifi.password);
  bootMode.begin(autoConnect, millis());
  sntpProvider.begin(wifi.enabled(), NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
  ntpProviderEnabled = wifi.enabled();
#else
  config.wifiEnabled = false;
  wifi.begin(false, nullptr, nullptr);
  diagnostics.setState(DiagnosticModule::NTP, DiagnosticState::STATE_DISABLED,
                       DiagnosticReason::DISABLED_BY_CONFIGURATION, millis());
#endif
}

void loop() {
  if (ppsPendingCount > 0) {
    noInterrupts();
    const uint32_t timestamp = ppsTimestampMillis;
    const uint8_t edgeCount = ppsPendingCount;
    ppsPendingCount = 0;
    interrupts();
    gps.notePps(timestamp, edgeCount);
  }
  gps.update();
  const uint32_t nowMillis = millis();
  const bool startupActive = startup.active(nowMillis);
  if (!startupActive && !hardwareCheck.started()) {
    hardwareCheck.begin(nowMillis, oledInitialized);
  }
  if (hardwareCheck.active()) {
    HardwareCheckInputs inputs;
    inputs.gpsNmeaAvailable = gps.data().nmeaAvailable;
    DiagnosticRecord si5351Diagnostic;
    diagnostics.read(DiagnosticModule::SI5351A, si5351Diagnostic);
    inputs.si5351State = si5351Diagnostic.state;
    hardwareCheck.update(nowMillis, inputs);
  }
  locatorService.update(config.fixedLocator, gps.data().positionValid,
                        gps.data().locator);
  const LocatorSnapshot locator = locatorService.snapshot();
  GpsTimeSample gpsTimeSample;
  gpsTimeSample.sourceAvailable = gps.data().nmeaAvailable;
  gpsTimeSample.utcValid = gps.data().timeValid;
  gpsTimeSample.ppsPresent = gps.data().ppsAvailable;
  gpsTimeSample.ppsAssociated = gps.data().ppsAssociated;
  gpsTimeSample.referenceMonotonicMillis = gps.data().lastSentenceMillis;
  gpsTimeSample.utcMillisOfDay =
      gps.utcMillisOfDayAt(gpsTimeSample.referenceMonotonicMillis);
  gpsTimeAdapter.update(timeService, gpsTimeSample);
  const bool networkTimingSafe =
      transmitter.state() != TransmitterState::PREPARING &&
      transmitter.state() != TransmitterState::TRANSMITTING;
  wifi.update(nowMillis, networkTimingSafe);
#if WSPR_HAS_WIFI
  bootMode.update(wifi.connected(),
                  wifi.state() == WifiConnectionState::STATE_FAILED,
                  nowMillis);
  if (bootMode.enterApRequested()) {
    rfSafety.disableOutput();
    transmitter.setTxAdmissionAllowed(false);
    bool randomReady = ProvisioningSecurity::generateNumericPassword(
        platformFillRandom, nullptr, provisioningPassword);
    uint32_t suffix = 0;
#if defined(ARDUINO_ARCH_ESP8266)
    suffix = ESP.getChipId() & 0x00ffffffUL;
#else
    suffix = static_cast<uint32_t>(ESP.getEfuseMac()) & 0x00ffffffUL;
#endif
    snprintf(provisioningSsid, sizeof(provisioningSsid),
             "WSPR-Beacon-%06lX", static_cast<unsigned long>(suffix));
    const bool apStarted = randomReady &&
        wifi.startProvisioningAp(provisioningSsid, provisioningPassword) &&
        wifi.apModeConfirmed();
    bootMode.apStartResult(apStarted);
    if (apStarted && apSetupBootState == ApSetupMarkerState::PENDING) {
      const ApSetupMarkerResult consumed = apSetupMarker.consume();
      if (consumed == ApSetupMarkerResult::OK)
        Serial.println(F("INFO AP_SETUP_MARKER_CONSUMED"));
      else
        Serial.println(F("ERR AP_SETUP_MARKER_CONSUME_FAILED RF_DISABLED"));
    }
    httpServer.setProfile(HttpTransportCore::Profile::PROVISIONING);
    if (apStarted) {
      serialConfigConsole.setProvisioningMode(
          true, provisioningSsid, provisioningPassword);
      Serial.print(F("PROVISIONING AP SSID: "));
      Serial.println(provisioningSsid);
      Serial.print(F("PROVISIONING AP PASSWORD: "));
      Serial.println(provisioningPassword);
      Serial.println(F("PROVISIONING URL: http://192.168.4.1/"));
    } else {
      Serial.println(F("ERR PROVISIONING_AP_START_FAILED RF_DISABLED"));
    }
  }
  if (wifi.enabled() != ntpProviderEnabled) {
    ntpProviderEnabled = wifi.enabled();
    sntpProvider.begin(ntpProviderEnabled, NTP_SERVER_PRIMARY,
                       NTP_SERVER_SECONDARY);
  }
  if (networkTimingSafe) sntpProvider.update(nowMillis, wifi.connected());
  const NtpProviderSnapshot ntp = sntpProvider.snapshot(nowMillis);
  timeService.setSourceAvailable(
      TimeSource::NTP,
      ntp.requestStarted && ntp.state != NtpProviderState::STATE_STALE);
  NtpTimeSample ntpSample;
  if (sntpProvider.takeSample(ntpSample)) {
    TimeReference reference;
    reference.utcMillisOfDay = ntpSample.utcMillisOfDay;
    reference.quality = TimeQuality::SYNCHRONIZED;
    reference.utcValid = true;
    timeService.update(TimeSource::NTP, reference,
                       ntpSample.referenceMonotonicMillis);
  }
#endif
  const TimeSnapshot applicationTime = timeService.snapshot(nowMillis);
  TimeSnapshot transmissionTime = applicationTime;
  if (!bootMode.timeAuthorizationAllowed(
          WsprScheduler::authorizationFor(transmissionTime))) {
    // An AP is not a time authority.  A formerly fresh NTP sample must not
    // authorize autonomous AP operation after the STA path has gone away.
    transmissionTime.statusFlags = 0;
    transmissionTime.source = TimeSource::NONE;
    transmissionTime.quality = TimeQuality::UNKNOWN;
  }
  const WsprTimeAuthorization timeAuthorization =
      transmitter.timeAuthorization(transmissionTime);
  configurationSessions.expireWeb(nowMillis, transmitter);
  uint32_t sessionMillisUntilSlot = 0;
  const bool sessionNextSlotKnown = WsprScheduler::millisUntilNextSlot(
      applicationTime, config.transmitIntervalMinutes,
      sessionMillisUntilSlot);
  configurationSessions.updateTxAdmission(
      timeAuthorization != WsprTimeAuthorization::NONE,
      rfSafety.operational() &&
          transmitter.operatingIdentityState() ==
              OperatingIdentityState::CONFIGURED,
      sessionNextSlotKnown, sessionMillisUntilSlot,
      transmitter);
  transmitter.setTxAdmissionAllowed(
      bootMode.rfOperationAllowed() &&
      manualTx.permitted() &&
      !wifiRestart.pending() &&
      !wifi.scanSnapshot().driverBusy &&
      configurationSessions.txAdmissionAllowed());
  if (bootMode.rfOperationAllowed() && startup.transmissionAllowed(nowMillis) &&
      hardwareCheck.transmissionAllowed()) {
    // Capture the millisecond and microsecond clocks together so the armed
    // slot keeps one coherent monotonic deadline on both time bases.
    const uint32_t txNowMillis = millis();
    const uint32_t txNowMicros = micros();
    transmitter.update(transmissionTime, locator, txNowMillis, txNowMicros);
  }
  if (transmitter.state() == TransmitterState::PREPARING) {
    uint32_t preparationMarginMillis = 0;
    const bool marginValid = transmitter.preparationMarginMillis(
        millis(), preparationMarginMillis);
    const TxDisplayAction displayAction = txDisplayCoordinator.update(
        transmitter.preparedSnapshot().valid, screen.available(), marginValid,
        preparationMarginMillis);
    if (displayAction == TxDisplayAction::RENDER_PREPARING) {
      screen.showPreparing(transmitter.preparedSnapshot());
    } else if (displayAction == TxDisplayAction::RENDER_TX_ON) {
      screen.showTxPrepared(transmitter.preparedSnapshot());
    }
    // Rendering is synchronous and attempted at most once. The RF path is
    // always released immediately, including absent OLED and insufficient margin.
    transmitter.releasePreparation();
  } else {
    txDisplayCoordinator.reset();
  }
#if WSPR_HAS_WIFI
  uint32_t httpMillisUntilSlot = 0;
  const bool httpNextSlotKnown = WsprScheduler::millisUntilNextSlot(
      applicationTime, config.transmitIntervalMinutes, httpMillisUntilSlot);
  HttpServiceGateInputs httpGateInputs;
  httpGateInputs.wifiConnected =
      bootMode.httpNetworkAvailable(wifi.connected(), wifi.apActive());
  httpGateInputs.rfOperational = rfSafety.operational();
  httpGateInputs.authorization = timeAuthorization;
  httpGateInputs.nextSlotKnown = httpNextSlotKnown;
  httpGateInputs.millisUntilSlot = httpMillisUntilSlot;
  httpGateInputs.configurationActive =
      configurationSessions.mode() == FirmwareOperatingMode::CONFIGURATION;
  if (transmitter.state() == TransmitterState::PREPARING) {
    httpGateInputs.rfPhase = HttpRfPhase::PREPARING;
  } else if (transmitter.state() == TransmitterState::TRANSMITTING) {
    httpGateInputs.rfPhase = HttpRfPhase::TRANSMITTING;
  }
  HttpServiceGate::Decision httpServiceDecision;
  if (bootMode.provisioning()) {
    httpServiceDecision.allowed =
        wifi.apActive() && !rfSafety.outputEnabled();
    httpServiceDecision.reason = !wifi.apActive()
        ? HttpGateCloseReason::WIFI_DISCONNECTED
        : rfSafety.outputEnabled() ? HttpGateCloseReason::RF_CRITICAL
                                   : HttpGateCloseReason::NONE;
  } else {
    httpServiceDecision = HttpServiceGate::evaluate(httpGateInputs);
  }
  currentHttpDecision = httpServiceDecision;
  diagnosticConsole.setHttpDiagnostics(&httpServer.diagnostics());
#endif
  updateRfDiagnostics(nowMillis);
  if (bootMode.rfOperationAllowed() && bandButton.pressed() &&
      transmitter.selectNextBand()) {
    Serial.print(F("Band: "));
    Serial.println(transmitter.bandName());
  }
  const OledDisplayTarget displayTarget =
      OledDisplayPolicy::select(bootMode.mode(), hardwareCheck.active());
  if (displayTarget == OledDisplayTarget::PROVISIONING) {
    screen.showProvisioning(provisioningSsid, provisioningPassword);
  } else if (displayTarget == OledDisplayTarget::PROVISIONING_ERROR) {
    screen.showProvisioning(nullptr, nullptr, true);
  } else if (displayTarget == OledDisplayTarget::HARDWARE_CHECK) {
    screen.showHardwareCheck(hardwareCheck.snapshot());
  } else {
    char wifiIpv4[16];
    wifi.localIpv4(wifiIpv4);
    OperationalStatusInputs displayInputs;
    displayInputs.nowMillis = nowMillis;
    displayInputs.time = applicationTime;
    displayInputs.locator = locator;
    displayInputs.authorization = timeAuthorization;
    displayInputs.wifiState = bootMode.apOperational()
        ? WifiConnectionState::STATE_AP_OPERATIONAL : wifi.state();
    displayInputs.wifiMode = bootMode.apOperational()
        ? WifiDisplayMode::AP
        : wifi.enabled() ? WifiDisplayMode::STA : WifiDisplayMode::OFF;
    displayInputs.wifiIpv4 = wifiIpv4;
    displayInputs.latitude = gps.data().latitude;
    displayInputs.longitude = gps.data().longitude;
    displayInputs.satellites = gps.data().satellites;
    displayInputs.gpsNmeaAvailable = gps.data().nmeaAvailable;
    displayInputs.gpsFixValid = gps.data().positionValid;
    displayInputs.wifiReconnecting = wifi.reconnecting();

    const WsprMessageSnapshot message = transmitter.messageSnapshot(locator);
    IdleStatusInputs idleInputs;
    idleInputs.time = applicationTime;
    idleInputs.callsign = message.callsign;
    idleInputs.locator = message.locator;
    idleInputs.band = transmitter.bandName();
    idleInputs.frequencyHz = transmitter.configuredFrequencyHz();
    idleInputs.frequencyMode = transmitter.frequencyMode();
    idleInputs.powerDbm = message.powerDbm;
    idleInputs.rfState =
                         transmitter.operatingIdentityState() !=
                             OperatingIdentityState::CONFIGURED ||
                         !bootMode.rfOperationAllowed() ||
                         !configurationSessions.txAdmissionAllowed()
        ? OledRfState::INHIBITED
        : timeAuthorization == WsprTimeAuthorization::NONE
            ? OledRfState::WAITING_TIME
            : rfSafety.operational() ? OledRfState::READY
                                     : OledRfState::INHIBITED;
    DiagnosticRecord si5351Diagnostic;
    if (diagnostics.read(DiagnosticModule::SI5351A, si5351Diagnostic) &&
        si5351Diagnostic.state == DiagnosticState::STATE_ERROR) {
      idleInputs.rfState = OledRfState::ERROR;
    }
    idleInputs.nextSlotValid = WsprScheduler::nextSlotUtcMillis(
        applicationTime, config.transmitIntervalMinutes,
        idleInputs.nextSlotUtcMillis);
    screen.update(displayInputs, idleInputs, transmitter, startupActive,
                  configurationSessions.snapshot(nowMillis));
  }
  const bool txBusy = transmitter.state() == TransmitterState::PREPARING ||
                      transmitter.state() == TransmitterState::TRANSMITTING;
  screen.servicePowerSave(nowMillis, txBusy);
  serialConfigConsole.update(txBusy, nowMillis, config, gps, timeService, locator,
                             transmitter, diagnosticConsole, wifi,
#if WSPR_HAS_WIFI
                             &configurationManager,
                             effectiveTxEligibility(nowMillis)
#else
                             nullptr, false
#endif
  );
#if WSPR_HAS_WIFI
  // Interactive responses above are never deferred. HTTP is serviced before
  // an optional periodic report, which waits until the active client is gone.
  const bool httpClientWasActive = httpServer.clientActive();
  httpServer.update(httpServiceDecision, millis());
  if (httpServer.takeWifiRestartResponseDelivered())
    wifiRestart.responseDelivered();
  if (wifiRestart.ready(millis(), httpServer.clientActive(),
                        transmitter.configurationSessionSafe() &&
                        !transmitter.rfOutputEnabled())) {
    transmitter.setTxAdmissionAllowed(false);
    rfSafety.disableOutput();
    ESP.restart();
    return;
  }
  const bool apOperationalResponseDelivered =
      httpServer.takeApOperationalTransitionDelivered();
  if (bootMode.apOperationalPending() && apOperationalResponseDelivered &&
      !httpServer.clientActive()) {
    rfSafety.disableOutput();
    transmitter.setTxAdmissionAllowed(false);
    const bool transitionSafe = wifi.apActive() &&
        transmitter.configurationSessionSafe() &&
        !transmitter.preparedSnapshot().valid &&
        !transmitter.rfOutputEnabled();
    if (!transitionSafe) {
      httpServer.transitionProfile(HttpTransportCore::Profile::PROVISIONING);
      bootMode.failApOperationalTransition();
      Serial.println(F("ERR AP_OPERATIONAL_TRANSITION_FAILED RF_DISABLED"));
    } else if (httpServer.transitionProfile(
                   HttpTransportCore::Profile::AP_OPERATIONAL)) {
      configurationSessions.activateResumeGuard(transmitter);
      if (!bootMode.completeApOperationalTransition()) {
        httpServer.transitionProfile(HttpTransportCore::Profile::PROVISIONING);
        bootMode.failApOperationalTransition();
        Serial.println(F("ERR AP_OPERATIONAL_TRANSITION_FAILED RF_DISABLED"));
      } else {
        serialConfigConsole.setNetworkMode(
            SerialNetworkMode::AP_OPERATIONAL,
            provisioningSsid, provisioningPassword);
        Serial.println(
            F("AP OPERATIONAL: http://192.168.4.1/ TX_RESUME_GUARD"));
      }
    } else {
      bootMode.failApOperationalTransition();
      Serial.println(F("ERR AP_OPERATIONAL_TRANSITION_FAILED RF_DISABLED"));
    }
  }
  const bool periodicReportAllowed =
      ConsoleWebCoordinator::periodicReportAllowed(
          serialConfigConsole.periodicDiagnosticsEnabled(),
          httpClientWasActive, httpServer.clientActive());
#else
  const bool periodicReportAllowed =
      serialConfigConsole.periodicDiagnosticsEnabled();
#endif
  // HTTP may record a gate transition using a later millis() sample. Capture
  // the diagnostic observation time afterwards so a same-loop report cannot
  // see the transition timestamp as being in the future.
  const uint32_t consoleNowMillis = millis();
  diagnosticConsole.update(periodicReportAllowed,
                           consoleNowMillis, config, gps, timeService, locator,
                           transmitter, wifi,
#if WSPR_HAS_WIFI
                           &configurationManager,
#else
                           nullptr,
#endif
                           serialConfigConsole.periodicDiagnosticsEnabled(),
                           serialConfigConsole.candidateDirty());
  StatusLedInputs ledInputs;
  ledInputs.startupActive = startupActive;
  ledInputs.hardwareTestActive = hardwareCheck.ledTestActive();
  ledInputs.rfError = !rfSafety.operational();
  ledInputs.transmitting =
      transmitter.state() == TransmitterState::TRANSMITTING;
  ledInputs.preparing = transmitter.state() == TransmitterState::PREPARING;
  ledInputs.wifiConfiguring =
      serialConfigConsole.wifiConfigurationActive() ||
      wifi.state() == WifiConnectionState::STATE_CONNECTING ||
      wifi.scanState() == WifiScanState::SCANNING;
  ledInputs.ready = transmitter.state() == TransmitterState::IDLE;
  const StatusLedOutput led = statusLed.update(nowMillis, ledInputs);
  static bool appliedStatusLed = false;
  const bool statusLedAllowed =
      !screen.powerSaveEnabled() || hardwareCheck.ledTestActive();
  const bool desiredStatusLed = statusLedAllowed && led.enabled;
  if (led.changed || desiredStatusLed != appliedStatusLed) {
    setStatusLed(desiredStatusLed);
    appliedStatusLed = desiredStatusLed;
  }
#if defined(ARDUINO_ARCH_ESP8266)
  TimeAuthorityLedInputs timeLedInputs;
  timeLedInputs.authorization = timeAuthorization;
  timeLedInputs.startupActive = startupActive;
  timeLedInputs.hardwareTestActive = hardwareCheck.ledTestActive();
  timeLedInputs.timingCritical = txBusy;
  const TimeAuthorityLedOutput timeLed =
      timeAuthorityLed.update(nowMillis, timeLedInputs);
  static bool appliedTimeLed = false;
  const bool desiredTimeLed = statusLedAllowed && timeLed.enabled;
  if (timeLed.changed || desiredTimeLed != appliedTimeLed) {
    setTimeAuthorityLed(desiredTimeLed);
    appliedTimeLed = desiredTimeLed;
  }
#endif
}
