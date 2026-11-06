#pragma once

#include <stddef.h>
#include <stdint.h>

#include "BeaconSettings.h"
#include "ConfigValidator.h"
#include "WifiCredentials.h"
#include "WsprFrequencySelector.h"

enum class ConfigCommand : uint8_t {
  NONE,
  HELP,
  MENU,
  STATUS,
  STATUS_GPS,
  STATUS_TIME,
  STATUS_RF,
  STATUS_WIFI,
  STATUS_MODULES,
  TX_STATUS,
  TX_LAST,
  TX_ON,
  TX_OFF,
  MONITOR_ON,
  MONITOR_OFF,
  RF_RECOVER,
  WSPR_SHOW,
  WSPR_SET,
  WSPR_FREQUENCY_MODE,
  CONFIG_ENTER,
  CONFIG_EXIT,
  CONFIG_SHOW,
  CONFIG_DISCARD,
  CONFIG_DEFAULTS,
  CONFIG_SAVE,
  FACTORY_RESET,
  WIFI_STATUS,
  WIFI_AP_INFO,
  WIFI_SCAN,
  WIFI_SSID,
  WIFI_PASSWORD,
  WIFI_CONNECT,
  WIFI_DISCONNECT
};
enum class CommandStatus : uint8_t {
  OK,
  EMPTY,
  UNKNOWN_COMMAND,
  MISSING_ARGUMENT,
  INVALID_VALUE,
  UNSUPPORTED
};
struct CommandResult {
  ConfigCommand command = ConfigCommand::NONE;
  CommandStatus status = CommandStatus::EMPTY;
  ConfigField field = ConfigField::CALLSIGN;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
};

class ConfigCommandParser {
 public:
  ConfigCommandParser();

  CommandResult execute(char *line);
  const BeaconSettings &candidate() const { return candidate_; }
  const WifiCredentials &wifiCredentials() const { return wifiCredentials_; }
  bool setWifiPassword(const char *value) {
    return wifiCredentials_.setPassword(value);
  }
  bool dirty() const;
  void loadCandidates(const BeaconSettings &wspr,
                      const WifiCredentials &wifi);
  void setCandidates(const BeaconSettings &wspr,
                     const WifiCredentials &wifi);

 private:
  BeaconSettings defaults_;
  BeaconSettings saved_;
  BeaconSettings candidate_;
  WifiCredentials savedWifi_;
  WifiCredentials wifiCredentials_;

  CommandResult executeWspr(char *arguments);
  CommandResult executeConfig(char *arguments);
  CommandResult executeWifi(char *arguments);
  CommandResult setValue(char *arguments);
};
