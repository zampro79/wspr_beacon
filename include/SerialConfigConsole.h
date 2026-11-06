#pragma once

#include <Arduino.h>

#include "BeaconConfig.h"
#include "CommandLineBuffer.h"
#include "ConsoleMode.h"
#include "ConsoleInputEcho.h"
#include "ConsoleMenu.h"
#include "ConfigCommandParser.h"
#include "ConfigurationManager.h"
#include "ConfigurationService.h"
#include "ConfigurationSessionController.h"
#include "DiagnosticConsole.h"
#include "GpsReceiver.h"
#include "LocatorService.h"
#include "ManualTxController.h"
#include "FactoryResetService.h"
#include "TimeService.h"
#include "WifiManager.h"
#include "WsprTransmitter.h"

enum class SerialNetworkMode : uint8_t {
  NORMAL_STA,
  AP_PROVISIONING,
  AP_OPERATIONAL
};

class SerialConfigConsole {
 public:
  explicit SerialConfigConsole(Stream &serial) : serial_(serial) {}

  void begin(ConfigurationManager *configuration,
             ConfigurationService *configurationService = nullptr,
             ConfigurationSessionController *configurationSessions = nullptr,
             ManualTxController *manualTx = nullptr,
             FactoryResetService *factoryResetService = nullptr);

  void update(bool txBusy, uint32_t nowMillis, const BeaconConfig &activeConfig,
              const GpsReceiver &gps, const TimeService &timeService,
              const LocatorSnapshot &locator,
              WsprTransmitter &transmitter,
              DiagnosticConsole &diagnosticConsole, WifiManager &wifi,
              ConfigurationManager *configuration,
              bool effectiveTxEligible = false);
  ConsoleMode mode() const { return mode_.mode(); }
  bool periodicDiagnosticsEnabled() const {
    return mode_.periodicDiagnosticsEnabled() && !menu_.active();
  }
  bool wifiConfigurationActive() const {
    return passwordExpected_ || menu_.passwordExpected();
  }
  bool candidateDirty() const { return parser_.dirty(); }
  bool cliConfigurationActive() const {
    return configurationSessions_ != nullptr &&
           configurationSessions_->cliActive();
  }
  void setProvisioningMode(bool active, const char *ssid = nullptr,
                           const char *password = nullptr) {
    setNetworkMode(active ? SerialNetworkMode::AP_PROVISIONING
                          : SerialNetworkMode::NORMAL_STA,
                   ssid, password);
  }
  void setNetworkMode(SerialNetworkMode mode, const char *ssid = nullptr,
                      const char *password = nullptr) {
    networkMode_ = mode;
    provisioningSsid_ = ssid;
    provisioningPassword_ = password;
  }

 private:
  static constexpr uint8_t MAX_BYTES_PER_UPDATE = 16;

  Stream &serial_;
  ConfigCommandParser parser_;
  CommandLineBuffer input_;
  ConsoleModeController mode_;
  ConsoleInputEcho echo_;
  ConsoleMenu menu_;
  bool pendingReady_ = false;
  bool droppedDuringTx_ = false;
  bool discardUntilTerminator_ = false;
  bool passwordExpected_ = false;
  bool scanReportPending_ = false;
  bool factoryResetConfirmationPending_ = false;
  uint32_t factoryResetConfirmationStartedMillis_ = 0;
  ConfigurationManager *configuration_ = nullptr;
  ConfigurationService *configurationService_ = nullptr;
  ConfigurationSessionController *configurationSessions_ = nullptr;
  ManualTxController *manualTx_ = nullptr;
  FactoryResetService *factoryResetService_ = nullptr;
  SerialNetworkMode networkMode_ = SerialNetworkMode::NORMAL_STA;
  const char *provisioningSsid_ = nullptr;
  const char *provisioningPassword_ = nullptr;

  void accept(char value, bool txBusy);
  ConsoleEchoAction echoInput(char value);
  void finishLine(LineInputResult inputResult, bool txBusy);
  void executePending(uint32_t nowMillis, const BeaconConfig &activeConfig,
                      const GpsReceiver &gps, const TimeService &timeService,
                      const LocatorSnapshot &locator,
                      WsprTransmitter &transmitter,
                      DiagnosticConsole &diagnosticConsole, WifiManager &wifi,
                      ConfigurationManager *configuration,
                      bool effectiveTxEligible);
  void printResult(const CommandResult &result);
  void printSettings(const char *heading);
  void printConfigurationStatus(const BeaconConfig &activeConfig) const;
  void printWifiStatus(const WifiManager &wifi);
  void printScanResults(WifiManager &wifi);
  void printPrompt();
  void printConfigurationMenu();
  void printMenu(const BeaconConfig &activeConfig);
  void printMenuValue(const BeaconConfig &activeConfig, ConfigField field);
};
