#include "SerialConfigConsole.h"

#include <string.h>

#include "BoardConfig.h"
#include "ConsoleConfigurationPolicy.h"
#include "ConsoleHelpText.h"

void SerialConfigConsole::begin(
    ConfigurationManager *configuration,
    ConfigurationService *configurationService,
    ConfigurationSessionController *configurationSessions,
    ManualTxController *manualTx,
    FactoryResetService *factoryResetService) {
  configuration_ = configuration;
  configurationService_ = configurationService;
  configurationSessions_ = configurationSessions;
  manualTx_ = manualTx;
  factoryResetService_ = factoryResetService;
#if WSPR_HAS_WIFI
  if (configuration_ != nullptr) {
    parser_.loadCandidates(configuration_->candidateWspr(),
                           configuration_->candidateWifi());
  }
#else
  (void)configuration;
#endif
  printPrompt();
}

void SerialConfigConsole::update(
    bool txBusy, uint32_t nowMillis, const BeaconConfig &activeConfig,
    const GpsReceiver &gps, const TimeService &timeService,
    const LocatorSnapshot &locator,
    WsprTransmitter &transmitter, DiagnosticConsole &diagnosticConsole,
    WifiManager &wifi, ConfigurationManager *configuration,
    bool effectiveTxEligible) {
  configuration_ = configuration;
  echo_.setSuspended(txBusy);
  if (!txBusy && factoryResetConfirmationPending_ &&
      static_cast<uint32_t>(nowMillis - factoryResetConfirmationStartedMillis_) >=
          30000UL) {
    factoryResetConfirmationPending_ = false;
    input_.clear();
    serial_.println(F("ERR FACTORY_RESET_CONFIRMATION_TIMEOUT CANCELLED"));
    printPrompt();
  }
  if (!txBusy && pendingReady_) {
    executePending(nowMillis, activeConfig, gps, timeService, locator, transmitter,
                   diagnosticConsole, wifi, configuration_,
                   effectiveTxEligible);
  }
  uint8_t consumed = 0;
  while (serial_.available() > 0 && consumed < MAX_BYTES_PER_UPDATE) {
    const char value = static_cast<char>(serial_.read());
    if (discardUntilTerminator_) {
      if (value == '\r' || value == '\n') discardUntilTerminator_ = false;
    } else if (txBusy) {
      if (value != '\r' && value != '\n') {
        droppedDuringTx_ = true;
        discardUntilTerminator_ = true;
      }
    } else if (pendingReady_) {
      discardUntilTerminator_ = value != '\r' && value != '\n';
    } else {
      mode_.noteInput(value);
      accept(value, false);
    }
    ++consumed;
    if (pendingReady_ && !txBusy) break;
  }

  if (txBusy) return;
  if (droppedDuringTx_) {
    serial_.println(F("ERR INPUT_DROPPED_DURING_TX"));
    droppedDuringTx_ = false;
    printPrompt();
  }
  if (!txBusy && !pendingReady_ && input_.length() == 0 && scanReportPending_ &&
      wifi.scanState() != WifiScanState::SCANNING) {
    printScanResults(wifi);
    printPrompt();
  }
  if (pendingReady_) {
    executePending(nowMillis, activeConfig, gps, timeService, locator, transmitter,
                   diagnosticConsole, wifi, configuration_,
                   effectiveTxEligible);
  }
}
void SerialConfigConsole::accept(char value, bool txBusy) {
  const ConsoleEchoAction echoAction = echoInput(value);
  const LineInputResult inputResult = input_.push(value);
  if (inputResult != LineInputResult::NONE) {
    finishLine(inputResult, txBusy);
  } else if (echoAction == ConsoleEchoAction::NEWLINE &&
             input_.length() == 0) {
    printPrompt();
  }
}

ConsoleEchoAction SerialConfigConsole::echoInput(char value) {
  const bool secret = passwordExpected_ || menu_.passwordExpected();
  const bool editable = !input_.overflowed() && input_.length() > 0;
  const bool appendable = !input_.overflowed() &&
      input_.length() + 1U < CommandLineBuffer::CAPACITY;
  const ConsoleEchoResult result =
      echo_.accept(value, secret, editable, appendable);
  switch (result.action) {
    case ConsoleEchoAction::NONE: break;
    case ConsoleEchoAction::CHARACTER: serial_.print(result.character); break;
    case ConsoleEchoAction::ERASE: serial_.print(F("\b \b")); break;
    case ConsoleEchoAction::NEWLINE: serial_.println(); break;
  }
  return result.action;
}
void SerialConfigConsole::finishLine(LineInputResult inputResult, bool txBusy) {
  if (inputResult == LineInputResult::OVERFLOW) {
    if (txBusy) {
      droppedDuringTx_ = true;
    } else {
      serial_.println(F("ERR LINE_TOO_LONG"));
      printPrompt();
    }
  } else if (inputResult == LineInputResult::READY) {
    if (pendingReady_) {
      droppedDuringTx_ = true;
    } else {
      pendingReady_ = true;
    }
  }
}

void SerialConfigConsole::executePending(
    uint32_t nowMillis, const BeaconConfig &activeConfig, const GpsReceiver &gps,
    const TimeService &timeService, const LocatorSnapshot &locator,
    WsprTransmitter &transmitter,
    DiagnosticConsole &diagnosticConsole, WifiManager &wifi,
    ConfigurationManager *configuration, bool effectiveTxEligible) {
  pendingReady_ = false;
  mode_.setMonitorEnabled(false);
  if (factoryResetConfirmationPending_) {
    const uint32_t elapsed =
        static_cast<uint32_t>(nowMillis - factoryResetConfirmationStartedMillis_);
    const bool exact = strcmp(input_.line(), "FACTORY RESET") == 0;
    const bool cancel = strcmp(input_.line(), "cancel") == 0;
    input_.clear();
    factoryResetConfirmationPending_ = false;
    if (cancel) {
      serial_.println(F("OK FACTORY_RESET_CANCELLED"));
    } else if (!exact) {
      serial_.println(F("ERR FACTORY_RESET_CONFIRMATION_REJECTED CANCELLED"));
    } else if (elapsed < 500UL) {
      serial_.println(F("ERR FACTORY_RESET_BUFFERED_INPUT_REJECTED CANCELLED"));
    } else if (factoryResetService_ == nullptr || configurationSessions_ == nullptr) {
      serial_.println(F("ERR FACTORY_RESET_UNAVAILABLE"));
    } else {
      const FactoryResetServiceResult reset = factoryResetService_->execute(
          FactoryResetOwner::CLI, nullptr, configurationSessions_->revision(),
          nowMillis);
      if (reset == FactoryResetServiceResult::COMPLETED) {
        parser_.loadCandidates(configuration_->candidateWspr(),
                               configuration_->candidateWifi());
        serial_.println(F("OK FACTORY_RESET_COMPLETED TX_INHIBITED RESTART_REQUIRED"));
      } else if (reset == FactoryResetServiceResult::OWNER_CONFLICT) {
        serial_.println(F("ERR FACTORY_RESET_WEB_OWNER_ACTIVE"));
      } else if (reset == FactoryResetServiceResult::OPERATION_BUSY) {
        serial_.println(F("ERR FACTORY_RESET_OPERATION_BUSY"));
      } else {
        serial_.println(F("ERR FACTORY_RESET_FAILED TX_INHIBITED"));
      }
    }
    printPrompt();
    return;
  }
  if (configurationService_ != nullptr) {
    parser_.setCandidates(configurationService_->manager().candidateWspr(),
                          parser_.wifiCredentials());
  }
  const BeaconSettings candidateBefore = parser_.candidate();
  const WifiCredentials wifiBefore = parser_.wifiCredentials();
  bool menuCommand = false;
  CommandResult result;
  const bool explicitConfigurationExit = cliConfigurationActive() &&
      (strcmp(input_.line(), "exit") == 0 ||
       strcmp(input_.line(), "config exit") == 0);
  if (menu_.active() && explicitConfigurationExit) {
    result.command = ConfigCommand::CONFIG_EXIT;
    result.status = CommandStatus::OK;
    input_.clear();
  } else if (menu_.active()) {
    const ConsoleMenuResult menuResult = menu_.accept(input_.line(), parser_);
    input_.clear();
    switch (menuResult.kind) {
      case ConsoleMenuResultKind::NAVIGATED:
        printMenu(activeConfig);
        return;
      case ConsoleMenuResultKind::VALUE_ACCEPTED:
        if (menuResult.command == ConfigCommand::WSPR_SET &&
            !cliConfigurationActive()) {
          parser_.setCandidates(candidateBefore, wifiBefore);
          serial_.println(F("ERR CONFIG_MODE_REQUIRED"));
          printPrompt();
          return;
        }
#if WSPR_HAS_WIFI
        if (configurationService_ != nullptr) {
          configurationService_->stageWspr(
              parser_.candidate(), configurationService_->revision());
          const WifiCredentials &wifiCandidate = parser_.wifiCredentials();
          if ((!wifiCandidate.hasSsid() && !wifiCandidate.hasPassword()) ||
              wifiCandidate.complete()) {
            configurationService_->stageWifi(
                wifiCandidate, configurationService_->revision());
          }
        }
#endif
        serial_.println(menuResult.command == ConfigCommand::WIFI_PASSWORD
                            ? F("OK PASSWORD_SET CANDIDATE_ONLY")
                            : F("OK CANDIDATE_SET ACTIVE_UNCHANGED"));
        printMenu(activeConfig);
        return;
      case ConsoleMenuResultKind::INVALID_SELECTION:
        serial_.println(F("ERR MENU_SELECTION_INVALID choose_listed_number"));
        printPrompt();
        return;
      case ConsoleMenuResultKind::INVALID_VALUE:
        serial_.println(F("ERR INVALID_VALUE candidate_unchanged"));
        printPrompt();
        return;
      case ConsoleMenuResultKind::SHOW_GPS_TIME:
        diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService,
                                   locator, transmitter, wifi, configuration,
                                   false, parser_.dirty(), ConsoleStatusView::GPS);
        diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService,
                                   locator, transmitter, wifi, configuration,
                                   false, parser_.dirty(), ConsoleStatusView::TIME);
        printMenu(activeConfig);
        return;
      case ConsoleMenuResultKind::EXITED:
        serial_.println(F("OK MENU_EXIT CANDIDATES_PRESERVED"));
        printPrompt();
        return;
      case ConsoleMenuResultKind::COMMAND:
        result.command = menuResult.command;
        result.status = menuResult.status;
        result.frequencyMode = menuResult.frequencyMode;
        menuCommand = true;
        break;
    }
  } else {
  if (passwordExpected_) {
    passwordExpected_ = false;
    const bool accepted = parser_.setWifiPassword(input_.line());
    input_.clear();
    serial_.println(accepted ? F("OK PASSWORD_SET CANDIDATE_ONLY")
                             : F("ERR INVALID_PASSWORD"));
    printPrompt();
    return;
  }
  result = parser_.execute(input_.line());
  }
  if (result.status == CommandStatus::OK &&
      !ConsoleConfigurationPolicy::allowed(result.command,
                                           cliConfigurationActive())) {
    parser_.setCandidates(candidateBefore, wifiBefore);
    serial_.println(F("ERR CONFIG_MODE_REQUIRED"));
    printPrompt();
    return;
  }
  if (result.status != CommandStatus::OK) {
    printResult(result);
    printPrompt();
    return;
  }
  if (factoryResetService_ != nullptr &&
      factoryResetService_->resetCompletedThisBoot() &&
      (result.command == ConfigCommand::CONFIG_ENTER ||
       ConsoleConfigurationPolicy::requiresOwnership(result.command))) {
    parser_.setCandidates(candidateBefore, wifiBefore);
    serial_.println(F("ERR FACTORY_RESET_COMPLETED RESTART_REQUIRED"));
    printPrompt();
    return;
  }
  if (networkMode_ == SerialNetworkMode::AP_PROVISIONING) {
    const bool readOnly = result.command == ConfigCommand::HELP ||
        result.command == ConfigCommand::STATUS ||
        result.command == ConfigCommand::STATUS_GPS ||
        result.command == ConfigCommand::STATUS_TIME ||
        result.command == ConfigCommand::STATUS_RF ||
        result.command == ConfigCommand::STATUS_WIFI ||
        result.command == ConfigCommand::STATUS_MODULES ||
        result.command == ConfigCommand::TX_STATUS ||
        result.command == ConfigCommand::TX_LAST ||
        result.command == ConfigCommand::TX_ON ||
        result.command == ConfigCommand::TX_OFF ||
        result.command == ConfigCommand::WIFI_STATUS ||
        result.command == ConfigCommand::WIFI_AP_INFO ||
        result.command == ConfigCommand::MONITOR_ON ||
        result.command == ConfigCommand::MONITOR_OFF;
    if (!readOnly) {
      parser_.setCandidates(candidateBefore, wifiBefore);
      serial_.println(F("ERR PROVISIONING_MODE_COMMAND_BLOCKED"));
      printPrompt();
      return;
    }
  }
  if (networkMode_ == SerialNetworkMode::AP_OPERATIONAL) {
    const bool blocked = result.command == ConfigCommand::MENU ||
        result.command == ConfigCommand::CONFIG_DEFAULTS ||
        result.command == ConfigCommand::WIFI_SCAN ||
        result.command == ConfigCommand::WIFI_SSID ||
        result.command == ConfigCommand::WIFI_PASSWORD ||
        result.command == ConfigCommand::WIFI_CONNECT ||
        result.command == ConfigCommand::WIFI_DISCONNECT ||
        result.command == ConfigCommand::RF_RECOVER;
    if (blocked) {
      parser_.setCandidates(candidateBefore, wifiBefore);
      serial_.println(F("ERR AP_OPERATIONAL_COMMAND_BLOCKED"));
      printPrompt();
      return;
    }
  }
  switch (result.command) {
    case ConfigCommand::HELP:
      serial_.println(F("[STATUS]"));
      serial_.println(F("status | status gps|time|rf|wifi|modules"));
      serial_.println(F("[CONSOLE]"));
      serial_.println(F("help | menu | config | exit | monitor on | monitor off"));
      serial_.println(F("[WSPR/RF]"));
      serial_.println(F("wspr show | wspr set <field> <value> | rf recover"));
      serial_.println(F(WSPR_FREQUENCY_MODE_HELP_TEXT));
      serial_.println(F("tx on | tx off | tx status | tx last"));
      serial_.println(F("fields: callsign locator band power interval frequency offset correction"));
      serial_.println(F("[CONFIGURATION]"));
  serial_.println(F("config | config show | config discard | config defaults | config save"));
      serial_.println(F("factory reset - erase WSPR and Wi-Fi after separate confirmation"));
      serial_.println(F("exit | config exit - leave configuration mode"));
      serial_.println(F("[NETWORK]"));
      serial_.println(F("wifi status | wifi ap | wifi scan | wifi ssid <name>"));
      serial_.println(F("wifi password | wifi connect | wifi disconnect"));
      serial_.println(configuration != nullptr
                          ? F("Persistence available; saved WSPR applies between frames")
                          : F("Persistence unavailable on this platform"));
      break;
    case ConfigCommand::CONFIG_ENTER: {
      if (configurationSessions_ == nullptr) {
        serial_.println(F("ERR CONFIG_MODE_UNAVAILABLE"));
        break;
      }
      const ConfigurationSessionResult sessionResult =
          configurationSessions_->acquireCli(transmitter);
      if (sessionResult == ConfigurationSessionResult::RF_BUSY) {
        serial_.println(F("ERR RF_BUSY CONFIG_MODE_UNCHANGED"));
        break;
      }
      if (sessionResult != ConfigurationSessionResult::OK &&
          sessionResult != ConfigurationSessionResult::UNCHANGED) {
        serial_.println(F("ERR CONFIG_MODE_UNAVAILABLE"));
        break;
      }
      serial_.println(sessionResult == ConfigurationSessionResult::UNCHANGED
                          ? F("OK CONFIG_MODE_ALREADY_ACTIVE")
                          : F("OK CONFIG_MODE_ENTERED TX_INHIBITED"));
      printConfigurationMenu();
      return;
    }
    case ConfigCommand::CONFIG_EXIT: {
      if (!cliConfigurationActive()) {
        serial_.println(F("ERR CONFIG_MODE_REQUIRED"));
        break;
      }
      configurationSessions_->releaseCli(transmitter);
      menu_.exit();
      serial_.println(F("OK CONFIG_MODE_EXIT CANDIDATES_PRESERVED"));
      break;
    }
    case ConfigCommand::MENU:
      menu_.enter();
      printMenu(activeConfig);
      return;
    case ConfigCommand::STATUS:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(), parser_.dirty());
      break;
    case ConfigCommand::STATUS_GPS:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::GPS);
      break;
    case ConfigCommand::STATUS_TIME:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::TIME);
      break;
    case ConfigCommand::STATUS_RF:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::RF);
      break;
    case ConfigCommand::STATUS_WIFI:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::WIFI);
      break;
    case ConfigCommand::STATUS_MODULES:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::MODULES);
      break;
    case ConfigCommand::TX_STATUS:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::TX_STATUS);
      serial_.print(F("manual_permission="));
      serial_.print(manualTx_ != nullptr && manualTx_->permitted()
                        ? F("TX_ON") : F("TX_OFF"));
      serial_.print(F(" effective_eligibility="));
      serial_.println(effectiveTxEligible
                          ? F("ELIGIBLE_FOR_FUTURE_SLOT")
                          : F("TX_INHIBITED"));
      break;
    case ConfigCommand::TX_LAST:
      diagnosticConsole.printNow(nowMillis, activeConfig, gps, timeService, locator,
                                 transmitter, wifi, configuration,
                                 mode_.periodicDiagnosticsEnabled(),
                                 parser_.dirty(),
                                 ConsoleStatusView::TX_LAST);
      break;
    case ConfigCommand::TX_OFF:
      if (manualTx_ == nullptr) serial_.println(F("ERR TX_CONTROL_UNAVAILABLE"));
      else {
        manualTx_->turnOff();
        serial_.println(F("OK TX_OFF MANUAL_INHIBIT_LATCHED RF_OUTPUT_DISABLED"));
      }
      break;
    case ConfigCommand::TX_ON:
      if (manualTx_ == nullptr) serial_.println(F("ERR TX_CONTROL_UNAVAILABLE"));
      else if (!manualTx_->turnOn(
                   networkMode_ == SerialNetworkMode::NORMAL_STA ||
                   networkMode_ == SerialNetworkMode::AP_OPERATIONAL)) {
        serial_.println(F("ERR TX_ON_NON_OPERATIONAL_MODE"));
      } else {
        serial_.println(F("OK TX_ON MANUAL_PERMISSION_ONLY"));
      }
      break;
    case ConfigCommand::MONITOR_ON:
      mode_.setMonitorEnabled(true);
      serial_.println(F("OK MONITOR_ON"));
      break;
    case ConfigCommand::MONITOR_OFF:
      serial_.println(F("OK MONITOR_OFF"));
      break;
    case ConfigCommand::RF_RECOVER:
      {
        const bool recovered = transmitter.recoverRf();
        if (manualTx_ != nullptr && !manualTx_->permitted()) {
          manualTx_->turnOff();
        }
        serial_.println(recovered ? F("OK RF_RECOVERED_MANUAL_STATE_UNCHANGED")
                                  : F("ERR RF_RECOVERY_FAILED"));
      }
      break;
    case ConfigCommand::WSPR_SHOW:
      printSettings("WSPR RAM CANDIDATE");
      break;
    case ConfigCommand::WSPR_SET:
#if WSPR_HAS_WIFI
      if (configurationService_ != nullptr &&
          configurationService_->stageWspr(
              parser_.candidate(), configurationService_->revision()) ==
              ConfigurationOperationResult::INVALID) {
        serial_.println(F("ERR INVALID_VALUE"));
      } else {
        serial_.println(F("OK CANDIDATE_SET TX_ACTIVE_UNCHANGED"));
      }
#else
      serial_.println(F("OK CANDIDATE_SET TX_ACTIVE_UNCHANGED"));
#endif
      break;
    case ConfigCommand::WSPR_FREQUENCY_MODE:
#if WSPR_HAS_WIFI
      if (configurationService_ != nullptr) {
        const ConfigurationOperationResult modeResult =
            configurationService_->selectFrequencyMode(
                result.frequencyMode, configurationService_->revision());
        if (modeResult == ConfigurationOperationResult::RF_BUSY) {
          parser_.setCandidates(
              configurationService_->manager().candidateWspr(),
              parser_.wifiCredentials());
          serial_.println(F("ERR RF_BUSY MODE_UNCHANGED"));
          break;
        }
        if (modeResult != ConfigurationOperationResult::OK &&
            modeResult != ConfigurationOperationResult::UNCHANGED) {
          parser_.setCandidates(
              configurationService_->manager().candidateWspr(),
              parser_.wifiCredentials());
          serial_.println(F("ERR INVALID_VALUE MODE_UNCHANGED"));
          break;
        }
        parser_.setCandidates(
            configurationService_->manager().candidateWspr(),
            parser_.wifiCredentials());
      } else
#endif
      if (!transmitter.setFrequencyMode(result.frequencyMode)) {
        serial_.println(F("ERR RF_BUSY MODE_UNCHANGED"));
      } else {
        serial_.print(F("OK FREQUENCY_MODE="));
        serial_.print(wsprFrequencyModeLabel(transmitter.frequencyMode()));
        serial_.println(F(" RAM_ONLY UNSAVED"));
      }
#if WSPR_HAS_WIFI
      if (configuration != nullptr) {
        serial_.print(F("OK FREQUENCY_MODE="));
        serial_.print(wsprFrequencyModeLabel(transmitter.frequencyMode()));
        serial_.println(F(" ACTIVE_CANDIDATE UNSAVED"));
      }
#endif
      break;
    case ConfigCommand::CONFIG_SHOW:
      printSettings("CONFIG CANDIDATE");
      printConfigurationStatus(activeConfig);
      break;
    case ConfigCommand::CONFIG_DISCARD:
#if WSPR_HAS_WIFI
      if (configuration != nullptr) {
        if (configurationService_ != nullptr) {
          configurationService_->discard(configurationService_->revision());
        } else {
          configuration->discardCandidates();
        }
        parser_.loadCandidates(configuration->candidateWspr(),
                               configuration->candidateWifi());
      }
#endif
      serial_.println(F("OK CANDIDATES_DISCARDED ACTIVE_UNCHANGED"));
      break;
    case ConfigCommand::CONFIG_DEFAULTS:
#if WSPR_HAS_WIFI
      if (configuration != nullptr) {
        if (configurationService_ != nullptr) {
          configurationService_->defaults(configurationService_->revision());
        } else {
          configuration->loadDefaultsAsCandidates();
        }
        parser_.setCandidates(configuration->candidateWspr(),
                              configuration->candidateWifi());
      }
#endif
      serial_.println(F("OK DEFAULT_CANDIDATES TX_ACTIVE_UNCHANGED"));
      break;
    case ConfigCommand::CONFIG_SAVE: {
#if WSPR_HAS_WIFI
      if (configuration == nullptr) {
        serial_.println(F("ERR PERSISTENCE_UNAVAILABLE"));
        break;
      }
      if (configurationService_ == nullptr) {
        serial_.println(F("ERR PERSISTENCE_UNAVAILABLE"));
        break;
      }
      ConfigurationOperationResult stageResult =
          configurationService_->stageWspr(
              parser_.candidate(), configurationService_->revision());
      if ((stageResult != ConfigurationOperationResult::OK &&
           stageResult != ConfigurationOperationResult::UNCHANGED) ||
          ((stageResult = configurationService_->stageWifi(
                parser_.wifiCredentials(), configurationService_->revision())) !=
               ConfigurationOperationResult::OK &&
           stageResult != ConfigurationOperationResult::UNCHANGED)) {
        serial_.println(F("ERR CONFIG_INVALID"));
        break;
      }
      {
      const bool wifiChanged = configuration->status().wifiDirty;
      const ConfigurationSaveOutcome saveOutcome =
          configurationService_->save(configurationService_->revision());
      switch (saveOutcome.result) {
        case ConfigurationOperationResult::OK:
          parser_.loadCandidates(configuration->candidateWspr(),
                                 configuration->candidateWifi());
#if WSPR_HAS_WIFI
          if (wifiChanged && configuration->activeWifi().complete()) {
            if (!wifi.connect(configuration->activeWifi().ssid,
                              configuration->activeWifi().password,
                              nowMillis)) {
              serial_.println(F("OK SAVED ERR WIFI_CONNECT_FAILED"));
              break;
            }
          } else if (wifiChanged) {
            wifi.disconnect(nowMillis);
          }
#endif
          serial_.println(F("OK CONFIG_SAVED ACTIVE_APPLIED"));
          break;
        case ConfigurationOperationResult::UNCHANGED:
          parser_.loadCandidates(configuration->candidateWspr(),
                                 configuration->candidateWifi());
          serial_.println(F("OK CONFIG_UNCHANGED ACTIVE_UNCHANGED"));
          break;
        case ConfigurationOperationResult::RF_BUSY:
          serial_.println(F("OK CONFIG_SAVE_DEFERRED"));
          break;
        case ConfigurationOperationResult::SAVE_PARTIAL:
          serial_.println(F("ERR CONFIG_SAVE_PARTIAL ACTIVE_UNCHANGED"));
          break;
        case ConfigurationOperationResult::RUNTIME_APPLY_FAILED:
          serial_.println(F("ERR CONFIG_SAVED_WSPR_APPLY_FAILED"));
          break;
        case ConfigurationOperationResult::INVALID:
        case ConfigurationOperationResult::STALE_REVISION:
        case ConfigurationOperationResult::SAVE_FAILED:
          serial_.println(F("ERR CONFIG_SAVE_FAILED ACTIVE_UNCHANGED"));
          break;
      }
      }
#else
      serial_.println(F("ERR PERSISTENCE_UNAVAILABLE"));
#endif
      break;
    }
    case ConfigCommand::FACTORY_RESET:
      if (factoryResetService_ == nullptr) {
        serial_.println(F("ERR FACTORY_RESET_UNAVAILABLE"));
        break;
      }
      if (configurationSessions_ != nullptr && configurationSessions_->webActive()) {
        serial_.println(F("ERR FACTORY_RESET_WEB_OWNER_ACTIVE"));
        break;
      }
      factoryResetConfirmationPending_ = true;
      factoryResetConfirmationStartedMillis_ = nowMillis;
      serial_.println(F("WARNING Factory Reset erases all WSPR settings and saved Wi-Fi credentials."));
      serial_.println(F("TX will be inhibited. No automatic reboot will occur."));
      serial_.println(F("Type FACTORY RESET within 30 seconds, or type cancel."));
      return;
    case ConfigCommand::WIFI_STATUS:
#if WSPR_HAS_WIFI
      printWifiStatus(wifi);
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::WIFI_AP_INFO:
      if (networkMode_ == SerialNetworkMode::NORMAL_STA ||
          provisioningSsid_ == nullptr ||
          provisioningPassword_ == nullptr) {
        serial_.println(F("ERR PROVISIONING_MODE_REQUIRED"));
      } else {
        serial_.print(F("AP SSID: ")); serial_.println(provisioningSsid_);
        serial_.print(F("AP PASSWORD: ")); serial_.println(provisioningPassword_);
        serial_.println(F("AP URL: http://192.168.4.1/"));
      }
      break;
    case ConfigCommand::WIFI_SCAN:
#if WSPR_HAS_WIFI
      if (wifi.startScan()) {
        scanReportPending_ = true;
        serial_.println(F("OK WIFI_SCAN_STARTED"));
      } else {
        serial_.println(F("ERR WIFI_SCAN_BUSY_OR_FAILED"));
      }
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::WIFI_SSID:
#if WSPR_HAS_WIFI
      serial_.println(F("OK SSID_SET CANDIDATE_ONLY"));
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::WIFI_PASSWORD:
#if WSPR_HAS_WIFI
      passwordExpected_ = true;
      serial_.println(F("PASSWORD>"));
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::WIFI_CONNECT:
#if WSPR_HAS_WIFI
      if (!parser_.wifiCredentials().complete()) {
        serial_.println(F("ERR WIFI_CREDENTIALS_INCOMPLETE"));
      } else if (wifi.connect(parser_.wifiCredentials().ssid,
                              parser_.wifiCredentials().password, nowMillis)) {
        serial_.println(F("OK WIFI_CONNECTING"));
      } else {
        serial_.println(F("ERR WIFI_CONNECT_FAILED"));
      }
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::WIFI_DISCONNECT:
#if WSPR_HAS_WIFI
      wifi.disconnect(nowMillis);
      serial_.println(F("OK WIFI_DISCONNECTED"));
#else
      serial_.println(F("ERR UNSUPPORTED"));
#endif
      break;
    case ConfigCommand::NONE:
      break;
  }
  if (menuCommand && menu_.active()) printMenu(activeConfig);
  else printPrompt();
}

void SerialConfigConsole::printWifiStatus(const WifiManager &wifi) {
  serial_.print(F("WIFI STATE="));
  switch (wifi.state()) {
    case WifiConnectionState::STATE_DISABLED: serial_.print(F("DISABLED")); break;
    case WifiConnectionState::STATE_DISCONNECTED: serial_.print(F("DISCONNECTED")); break;
    case WifiConnectionState::STATE_CONNECTING: serial_.print(F("CONNECTING")); break;
    case WifiConnectionState::STATE_CONNECTED: serial_.print(F("CONNECTED")); break;
    case WifiConnectionState::STATE_FAILED: serial_.print(F("FAILED")); break;
    case WifiConnectionState::STATE_AP_OPERATIONAL:
      serial_.print(F("AP_OPERATIONAL")); break;
  }
  serial_.print(F(" CANDIDATE_SSID="));
  if (parser_.wifiCredentials().hasSsid()) {
    serial_.print(parser_.wifiCredentials().ssid);
  } else {
    serial_.print(F("<not set>"));
  }
  serial_.print(F(" PASSWORD="));
  serial_.println(parser_.wifiCredentials().hasPassword() ? F("SET")
                                                           : F("NOT_SET"));
}

void SerialConfigConsole::printScanResults(WifiManager &wifi) {
  scanReportPending_ = false;
  if (wifi.scanState() == WifiScanState::FAILED) {
    serial_.println(F("ERR WIFI_SCAN_FAILED"));
    wifi.consumeScanResults();
    return;
  }
  serial_.print(F("WIFI SCAN COUNT="));
  serial_.println(wifi.scanCount());
  for (uint8_t index = 0; index < wifi.scanCount(); ++index) {
    const WifiScanEntry &entry = wifi.scanEntry(index);
    serial_.print(index + 1);
    serial_.print(F(" SSID="));
    serial_.print(entry.ssid);
    serial_.print(F(" RSSI="));
    serial_.print(entry.rssi);
    serial_.print(F(" SECURITY="));
    serial_.println(entry.encrypted ? F("SECURED") : F("OPEN"));
  }
  wifi.consumeScanResults();
}

void SerialConfigConsole::printResult(const CommandResult &result) {
  serial_.print(F("ERR "));
  switch (result.status) {
    case CommandStatus::EMPTY: serial_.print(F("EMPTY command_required")); break;
    case CommandStatus::UNKNOWN_COMMAND:
      serial_.print(F("UNKNOWN_COMMAND use_help")); break;
    case CommandStatus::MISSING_ARGUMENT:
      serial_.print(F("ARGUMENTS missing_or_extra")); break;
    case CommandStatus::INVALID_VALUE:
      serial_.print(F("INVALID_VALUE check_help_and_limits")); break;
    case CommandStatus::UNSUPPORTED:
      serial_.print(F("UNSUPPORTED on_this_platform")); break;
    case CommandStatus::OK: serial_.print(F("OK")); break;
  }
  serial_.println();
}

void SerialConfigConsole::printPrompt() {
  if (passwordExpected_ || !mode_.promptAllowed(true, input_.length() != 0)) return;
  switch (menu_.screen()) {
    case ConsoleMenuScreen::MAIN: serial_.print(F("Selezione [0-9]: ")); break;
    case ConsoleMenuScreen::WSPR: serial_.print(F("Selezione WSPR [0-8]: ")); break;
    case ConsoleMenuScreen::WSPR_FREQUENCY_MODE:
      serial_.print(F("Frequency mode [0-2]: ")); break;
    case ConsoleMenuScreen::WIFI: serial_.print(F("Selezione Wi-Fi [0-6]: ")); break;
    case ConsoleMenuScreen::CALIBRATION:
      serial_.print(F("Selezione calibrazione [0-1]: ")); break;
    case ConsoleMenuScreen::WSPR_VALUE:
      serial_.print(F("Nuovo valore: ")); break;
    case ConsoleMenuScreen::WIFI_SSID:
      serial_.print(F("Nuovo SSID: ")); break;
    case ConsoleMenuScreen::WIFI_PASSWORD:
      serial_.print(F("Nuova password (non visualizzata): ")); break;
    case ConsoleMenuScreen::CALIBRATION_VALUE:
      serial_.print(F("Nuova correzione: ")); break;
    case ConsoleMenuScreen::INACTIVE:
      serial_.print(cliConfigurationActive() ? F("config> ") : F("wspr> "));
      break;
  }
}

void SerialConfigConsole::printConfigurationMenu() {
  serial_.println(F("WSPR CONFIGURATION"));
  serial_.println(F("------------------"));
  serial_.println(F("Show: config show"));
  serial_.println(F("Set:  wspr set callsign|locator|band|frequency|power|interval|offset|correction <value>"));
  serial_.println(F("Power Save: wspr set power-save on|off"));
  serial_.println(F("Bands: 40m|30m|20m|17m|15m|12m|10m"));
  serial_.println(F("Mode: wspr frequency mode fixed|random"));
  serial_.println(F("Save: config save    Discard: config discard"));
  serial_.println(F("Defaults: config defaults"));
  serial_.println(F("Exit: exit | config exit | 0"));
  printPrompt();
}

void SerialConfigConsole::printMenuValue(const BeaconConfig &activeConfig,
                                         ConfigField field) {
  const BeaconSettings &candidate = parser_.candidate();
  serial_.print(F("ACTIVE="));
  switch (field) {
    case ConfigField::CALLSIGN:
      if (activeConfig.callsign[0] == '\0') serial_.print(F("Not configured"));
      else serial_.print(activeConfig.callsign);
      break;
    case ConfigField::LOCATOR: serial_.print(activeConfig.fixedLocator); break;
    case ConfigField::BAND: {
      const BandPlanEntry *band = BandPlan::find(activeConfig.band);
      serial_.print(band == nullptr ? "--" : band->name);
      break;
    }
    case ConfigField::POWER_DBM: serial_.print(activeConfig.powerDbm); break;
    case ConfigField::INTERVAL_MINUTES:
      serial_.print(activeConfig.transmitIntervalMinutes); break;
    case ConfigField::BASE_FREQUENCY_HZ:
      serial_.print(activeConfig.baseFrequencyHz); break;
    case ConfigField::FREQUENCY_OFFSET_HZ:
      serial_.print(activeConfig.frequencyOffsetHz); break;
    case ConfigField::SI5351_CORRECTION:
      serial_.print(activeConfig.si5351Correction); break;
    case ConfigField::POWER_SAVE:
      serial_.print(configuration_ != nullptr &&
                            configuration_->activeWspr().powerSave
                        ? F("ON") : F("OFF"));
      break;
  }
  serial_.print(F(" CANDIDATE="));
  switch (field) {
    case ConfigField::CALLSIGN:
      if (candidate.callsign[0] == '\0') serial_.println(F("Not configured"));
      else serial_.println(candidate.callsign);
      break;
    case ConfigField::LOCATOR: serial_.println(candidate.fixedLocator); break;
    case ConfigField::BAND: {
      const BandPlanEntry *band = BandPlan::find(candidate.band);
      serial_.println(band == nullptr ? "--" : band->name);
      break;
    }
    case ConfigField::POWER_DBM: serial_.println(candidate.powerDbm); break;
    case ConfigField::INTERVAL_MINUTES:
      serial_.println(candidate.transmitIntervalMinutes); break;
    case ConfigField::BASE_FREQUENCY_HZ:
      serial_.println(candidate.baseFrequencyHz); break;
    case ConfigField::FREQUENCY_OFFSET_HZ:
      serial_.println(candidate.frequencyOffsetHz); break;
    case ConfigField::SI5351_CORRECTION:
      serial_.println(candidate.si5351Correction); break;
    case ConfigField::POWER_SAVE:
      serial_.println(candidate.powerSave ? F("ON") : F("OFF")); break;
  }
}

void SerialConfigConsole::printMenu(const BeaconConfig &activeConfig) {
  switch (menu_.screen()) {
    case ConsoleMenuScreen::MAIN:
      serial_.println(F("\n=== WSPR BEACON MENU ==="));
      serial_.println(F("1 Configurazione WSPR"));
      serial_.println(F("2 Configurazione Wi-Fi"));
      serial_.println(F("3 Stato GPS e sincronizzazione"));
      serial_.println(F("4 Calibrazione Si5351A"));
      serial_.println(F("5 Diagnostica moduli"));
      serial_.println(F("6 Stato generale del beacon"));
      serial_.println(F("7 Salva configurazione"));
      serial_.println(F("8 Annulla modifiche"));
      serial_.println(F("9 Esci (0=esci)"));
      break;
    case ConsoleMenuScreen::WSPR:
      serial_.println(F("\n--- CONFIGURAZIONE WSPR ---"));
      serial_.print(F("1 Nominativo: ")); printMenuValue(activeConfig, ConfigField::CALLSIGN);
      serial_.print(F("2 Locator: ")); printMenuValue(activeConfig, ConfigField::LOCATOR);
      serial_.print(F("3 Potenza dBm: ")); printMenuValue(activeConfig, ConfigField::POWER_DBM);
      serial_.print(F("4 Banda: ")); printMenuValue(activeConfig, ConfigField::BAND);
      serial_.print(F("5 Frequenza Hz: ")); printMenuValue(activeConfig, ConfigField::BASE_FREQUENCY_HZ);
      serial_.print(F("6 Offset Hz: ")); printMenuValue(activeConfig, ConfigField::FREQUENCY_OFFSET_HZ);
      serial_.print(F("7 Intervallo min: ")); printMenuValue(activeConfig, ConfigField::INTERVAL_MINUTES);
      serial_.print(F("8 Frequency mode: ACTIVE="));
      serial_.print(wsprFrequencyModeLabel(activeConfig.frequencyMode));
      serial_.print(F(" CANDIDATE="));
      serial_.println(wsprFrequencyModeLabel(parser_.candidate().frequencyMode));
      serial_.println(F("0 Indietro"));
      break;
    case ConsoleMenuScreen::WSPR_FREQUENCY_MODE:
      serial_.print(F("ACTIVE="));
      serial_.print(wsprFrequencyModeLabel(activeConfig.frequencyMode));
      serial_.print(F(" CANDIDATE="));
      serial_.println(wsprFrequencyModeLabel(parser_.candidate().frequencyMode));
      serial_.println(F("1 FIXED"));
      serial_.println(F("2 RANDOM"));
      serial_.println(F("0 Back"));
      serial_.println(F("Changes remain unsaved until config save."));
      break;
    case ConsoleMenuScreen::WIFI:
      serial_.println(F("\n--- CONFIGURAZIONE WI-FI ---"));
      serial_.println(F("1 Stato"));
      serial_.print(F("2 SSID ACTIVE="));
#if WSPR_HAS_WIFI
      serial_.print(configuration_ != nullptr &&
                            configuration_->activeWifi().hasSsid()
                        ? configuration_->activeWifi().ssid : "<not set>");
#else
      serial_.print(F("<not set>"));
#endif
      serial_.print(F(" CANDIDATE="));
      serial_.println(parser_.wifiCredentials().hasSsid()
                          ? parser_.wifiCredentials().ssid : "<not set>");
      serial_.print(F("3 Password ACTIVE="));
#if WSPR_HAS_WIFI
      serial_.print(configuration_ != nullptr &&
                            configuration_->activeWifi().hasPassword()
                        ? F("SET") : F("NOT_SET"));
#else
      serial_.print(F("NOT_SET"));
#endif
      serial_.print(F(" CANDIDATE="));
      serial_.println(parser_.wifiCredentials().hasPassword() ? F("SET")
                                                               : F("NOT_SET"));
      serial_.println(F("4 Scansione reti"));
      serial_.println(F("5 Connetti"));
      serial_.println(F("6 Disconnetti"));
      serial_.println(F("0 Indietro"));
      break;
    case ConsoleMenuScreen::CALIBRATION:
      serial_.println(F("\n--- CALIBRAZIONE SI5351A ---"));
      serial_.print(F("1 Correzione: "));
      printMenuValue(activeConfig, ConfigField::SI5351_CORRECTION);
      serial_.println(F("0 Indietro"));
      break;
    case ConsoleMenuScreen::WSPR_VALUE:
    case ConsoleMenuScreen::CALIBRATION_VALUE:
      printMenuValue(activeConfig, menu_.pendingField());
      break;
    case ConsoleMenuScreen::WIFI_SSID:
      serial_.print(F("SSID ACTIVE="));
#if WSPR_HAS_WIFI
      serial_.print(configuration_ != nullptr &&
                            configuration_->activeWifi().hasSsid()
                        ? configuration_->activeWifi().ssid : "<not set>");
#else
      serial_.print(F("<not set>"));
#endif
      serial_.print(F(" CANDIDATE="));
      serial_.println(parser_.wifiCredentials().hasSsid()
                          ? parser_.wifiCredentials().ssid : "<not set>");
      break;
    case ConsoleMenuScreen::WIFI_PASSWORD:
      serial_.print(F("Password ACTIVE="));
#if WSPR_HAS_WIFI
      serial_.print(configuration_ != nullptr &&
                            configuration_->activeWifi().hasPassword()
                        ? F("SET") : F("NOT_SET"));
#else
      serial_.print(F("NOT_SET"));
#endif
      serial_.print(F(" CANDIDATE="));
      serial_.println(parser_.wifiCredentials().hasPassword() ? F("SET")
                                                               : F("NOT_SET"));
      break;
    case ConsoleMenuScreen::INACTIVE:
      break;
  }
  printPrompt();
}

void SerialConfigConsole::printSettings(const char *heading) {
  const BeaconSettings &settings = parser_.candidate();
  serial_.println(heading);
  serial_.print(F("callsign="));
  if (settings.callsign[0] == '\0') serial_.println(F("Not configured"));
  else serial_.println(settings.callsign);
  serial_.print(F("locator=")); serial_.println(settings.fixedLocator);
  serial_.print(F("power_dbm=")); serial_.println(settings.powerDbm);
  serial_.print(F("interval_min=")); serial_.println(settings.transmitIntervalMinutes);
  serial_.print(F("base_hz=")); serial_.println(settings.baseFrequencyHz);
  serial_.print(F("offset_hz=")); serial_.println(settings.frequencyOffsetHz);
  serial_.print(F("correction=")); serial_.println(settings.si5351Correction);
  const BandPlanEntry *band = BandPlan::find(settings.band);
  serial_.print(F("band=")); serial_.println(band == nullptr ? "--" : band->name);
  serial_.print(F("frequency_mode="));
  serial_.println(wsprFrequencyModeLabel(settings.frequencyMode));
  serial_.print(F("power_save="));
  serial_.println(settings.powerSave ? F("ON") : F("OFF"));
  serial_.print(F("dirty=")); serial_.println(parser_.dirty() ? F("YES") : F("NO"));
  serial_.println(configuration_ != nullptr
                      ? F("persistence=V3 apply=BETWEEN_FRAMES")
                      : F("persistence=UNAVAILABLE apply=UNAVAILABLE"));
}

void SerialConfigConsole::printConfigurationStatus(
    const BeaconConfig &activeConfig) const {
#if WSPR_HAS_WIFI
  if (configuration_ == nullptr) return;
  const ConfigurationStatus status = configuration_->status();
  const BeaconSettings &saved = configuration_->savedWspr();
  serial_.print(F("wspr_origin="));
  serial_.println(status.wsprOrigin == ConfigOrigin::SAVED ? F("SAVED")
                                                            : F("DEFAULT"));
  serial_.print(F("wifi_origin="));
  serial_.println(status.wifiOrigin == ConfigOrigin::SAVED ? F("SAVED")
                                                             : F("DEFAULT"));
  serial_.print(F("candidate_dirty="));
  serial_.println((status.wsprDirty || status.wifiDirty || parser_.dirty())
                      ? F("YES")
                      : F("NO"));
  serial_.print(F("wifi_saved="));
  serial_.println(configuration_->savedWifi().complete() ? F("SET")
                                                          : F("NOT_SET"));
  serial_.print(F("wifi_active="));
  serial_.println(configuration_->activeWifi().complete() ? F("SET")
                                                           : F("NOT_SET"));
  serial_.print(F("wspr_saved_callsign="));
  if (saved.callsign[0] == '\0') serial_.println(F("Not configured"));
  else serial_.println(saved.callsign);
  serial_.print(F("wspr_saved_locator=")); serial_.println(saved.fixedLocator);
  serial_.print(F("wspr_saved_power_dbm=")); serial_.println(saved.powerDbm);
  serial_.print(F("wspr_saved_interval_min=")); serial_.println(saved.transmitIntervalMinutes);
  serial_.print(F("wspr_saved_base_hz=")); serial_.println(saved.baseFrequencyHz);
  serial_.print(F("wspr_saved_offset_hz=")); serial_.println(saved.frequencyOffsetHz);
  serial_.print(F("wspr_saved_correction=")); serial_.println(saved.si5351Correction);
  const BandPlanEntry *savedBand = BandPlan::find(saved.band);
  serial_.print(F("wspr_saved_band="));
  serial_.println(savedBand == nullptr ? "--" : savedBand->name);
  serial_.print(F("wspr_saved_frequency_mode="));
  serial_.println(wsprFrequencyModeLabel(saved.frequencyMode));
  serial_.print(F("wspr_saved_power_save="));
  serial_.println(saved.powerSave ? F("ON") : F("OFF"));
  serial_.print(F("wspr_active_callsign="));
  if (activeConfig.callsign[0] == '\0') serial_.println(F("Not configured"));
  else serial_.println(activeConfig.callsign);
  serial_.print(F("wspr_active_locator=")); serial_.println(activeConfig.fixedLocator);
  serial_.print(F("wspr_active_power_dbm=")); serial_.println(activeConfig.powerDbm);
  serial_.print(F("wspr_active_interval_min=")); serial_.println(activeConfig.transmitIntervalMinutes);
  serial_.print(F("wspr_active_base_hz=")); serial_.println(activeConfig.baseFrequencyHz);
  serial_.print(F("wspr_active_offset_hz=")); serial_.println(activeConfig.frequencyOffsetHz);
  serial_.print(F("wspr_active_correction=")); serial_.println(activeConfig.si5351Correction);
  const BandPlanEntry *activeBand = BandPlan::find(activeConfig.band);
  serial_.print(F("wspr_active_band="));
  serial_.println(activeBand == nullptr ? "--" : activeBand->name);
  serial_.print(F("wspr_active_frequency_mode="));
  serial_.println(wsprFrequencyModeLabel(activeConfig.frequencyMode));
  serial_.print(F("wspr_active_power_save="));
  serial_.println(configuration_->activeWspr().powerSave ? F("ON") : F("OFF"));
  serial_.print(F("wspr_payload_version="));
  serial_.println(status.wsprPayloadVersion);
  serial_.print(F("wspr_migration_pending="));
  serial_.println(status.wsprMigrationPending ? F("YES") : F("NO"));
#endif
}
