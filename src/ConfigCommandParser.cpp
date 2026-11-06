#include "ConfigCommandParser.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace {
char *skipSpaces(char *text) {
  while (*text == ' ' || *text == '\t') ++text;
  return text;
}
char *nextToken(char *&text) {
  text = skipSpaces(text);
  if (*text == '\0') return nullptr;
  char *token = text;
  while (*text != '\0' && *text != ' ' && *text != '\t') ++text;
  if (*text != '\0') *text++ = '\0';
  return token;
}
void uppercase(char *text) {
  while (*text != '\0') {
    *text = static_cast<char>(toupper(static_cast<unsigned char>(*text)));
    ++text;
  }
}

bool parseLong(const char *text, long &value) {
  if (text == nullptr || *text == '\0') return false;
  errno = 0;
  char *end = nullptr;
  value = strtol(text, &end, 10);
  return errno == 0 && end != text && *end == '\0';
}

CommandResult result(ConfigCommand command, CommandStatus status,
                     ConfigField field = ConfigField::CALLSIGN) {
  CommandResult response;
  response.command = command;
  response.status = status;
  response.field = field;
  return response;
}

CommandResult frequencyModeResult(WsprFrequencyMode mode) {
  CommandResult response =
      result(ConfigCommand::WSPR_FREQUENCY_MODE, CommandStatus::OK);
  response.frequencyMode = mode;
  return response;
}

ConfigValidation validateCandidate(const BeaconSettings &settings) {
  if (settings.callsign[0] != '\0') return ConfigValidator::validate(settings);
  BeaconSettings withValidatedIdentity = settings;
  strcpy(withValidatedIdentity.callsign, "AA0AA");
  return ConfigValidator::validate(withValidatedIdentity);
}
}  // namespace

ConfigCommandParser::ConfigCommandParser()
    : defaults_(), saved_(defaults_), candidate_(defaults_) {}

void ConfigCommandParser::loadCandidates(const BeaconSettings &wspr,
                                         const WifiCredentials &wifi) {
  saved_ = candidate_ = wspr;
  savedWifi_ = wifiCredentials_ = wifi;
}

void ConfigCommandParser::setCandidates(const BeaconSettings &wspr,
                                        const WifiCredentials &wifi) {
  candidate_ = wspr;
  wifiCredentials_ = wifi;
}

bool ConfigCommandParser::dirty() const {
  return strcmp(candidate_.callsign, saved_.callsign) != 0 ||
         strcmp(candidate_.fixedLocator, saved_.fixedLocator) != 0 ||
         candidate_.baseFrequencyHz != saved_.baseFrequencyHz ||
         candidate_.frequencyOffsetHz != saved_.frequencyOffsetHz ||
         candidate_.si5351Correction != saved_.si5351Correction ||
         candidate_.powerDbm != saved_.powerDbm ||
         candidate_.transmitIntervalMinutes != saved_.transmitIntervalMinutes ||
         candidate_.band != saved_.band ||
         candidate_.frequencyMode != saved_.frequencyMode ||
         candidate_.powerSave != saved_.powerSave ||
         strcmp(wifiCredentials_.ssid, savedWifi_.ssid) != 0 ||
         strcmp(wifiCredentials_.password, savedWifi_.password) != 0 ||
         wifiCredentials_.open != savedWifi_.open;
}

CommandResult ConfigCommandParser::execute(char *line) {
  if (line == nullptr) return result(ConfigCommand::NONE, CommandStatus::EMPTY);
  char *cursor = line;
  char *command = nextToken(cursor);
  if (command == nullptr) return result(ConfigCommand::NONE, CommandStatus::EMPTY);
  if (strcmp(command, "help") == 0) {
    return result(ConfigCommand::HELP,
                  nextToken(cursor) == nullptr ? CommandStatus::OK
                                               : CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "menu") == 0) {
    return result(ConfigCommand::MENU,
                  nextToken(cursor) == nullptr ? CommandStatus::OK
                                               : CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "status") == 0) {
    char *detail = nextToken(cursor);
    if (detail == nullptr) return result(ConfigCommand::STATUS, CommandStatus::OK);
    if (nextToken(cursor) != nullptr) {
      return result(ConfigCommand::STATUS, CommandStatus::UNKNOWN_COMMAND);
    }
    if (strcmp(detail, "gps") == 0) {
      return result(ConfigCommand::STATUS_GPS, CommandStatus::OK);
    }
    if (strcmp(detail, "time") == 0) {
      return result(ConfigCommand::STATUS_TIME, CommandStatus::OK);
    }
    if (strcmp(detail, "rf") == 0) {
      return result(ConfigCommand::STATUS_RF, CommandStatus::OK);
    }
    if (strcmp(detail, "wifi") == 0) {
      return result(ConfigCommand::STATUS_WIFI, CommandStatus::OK);
    }
    if (strcmp(detail, "modules") == 0) {
      return result(ConfigCommand::STATUS_MODULES, CommandStatus::OK);
    }
    return result(ConfigCommand::STATUS, CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "monitor") == 0) {
    char *operation = nextToken(cursor);
    if (operation == nullptr || nextToken(cursor) != nullptr) {
      return result(ConfigCommand::NONE, CommandStatus::MISSING_ARGUMENT);
    }
    if (strcmp(operation, "on") == 0) {
      return result(ConfigCommand::MONITOR_ON, CommandStatus::OK);
    }
    if (strcmp(operation, "off") == 0) {
      return result(ConfigCommand::MONITOR_OFF, CommandStatus::OK);
    }
    return result(ConfigCommand::NONE, CommandStatus::INVALID_VALUE);
  }
  if (strcmp(command, "rf") == 0) {
    char *operation = nextToken(cursor);
    if (operation != nullptr && strcmp(operation, "recover") == 0 &&
        nextToken(cursor) == nullptr) {
      return result(ConfigCommand::RF_RECOVER, CommandStatus::OK);
    }
    return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "tx") == 0) {
    char *operation = nextToken(cursor);
    if (operation == nullptr || nextToken(cursor) != nullptr) {
      return result(ConfigCommand::NONE, CommandStatus::MISSING_ARGUMENT);
    }
    if (strcmp(operation, "status") == 0) {
      return result(ConfigCommand::TX_STATUS, CommandStatus::OK);
    }
    if (strcmp(operation, "last") == 0) {
      return result(ConfigCommand::TX_LAST, CommandStatus::OK);
    }
    if (strcmp(operation, "on") == 0) {
      return result(ConfigCommand::TX_ON, CommandStatus::OK);
    }
    if (strcmp(operation, "off") == 0) {
      return result(ConfigCommand::TX_OFF, CommandStatus::OK);
    }
    return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "wspr") == 0) return executeWspr(cursor);
  if (strcmp(command, "exit") == 0 || strcmp(command, "0") == 0) {
    return result(ConfigCommand::CONFIG_EXIT,
                  nextToken(cursor) == nullptr ? CommandStatus::OK
                                               : CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "config") == 0) return executeConfig(cursor);
  if (strcmp(command, "factory") == 0) {
    char *operation = nextToken(cursor);
    return result(ConfigCommand::FACTORY_RESET,
        operation != nullptr && strcmp(operation, "reset") == 0 &&
        nextToken(cursor) == nullptr ? CommandStatus::OK
                                     : CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(command, "wifi") == 0) return executeWifi(cursor);
  return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
}

CommandResult ConfigCommandParser::executeWifi(char *arguments) {
  char *operation = nextToken(arguments);
  if (operation == nullptr) {
    return result(ConfigCommand::NONE, CommandStatus::MISSING_ARGUMENT);
  }
  if (strcmp(operation, "ssid") == 0) {
    char *ssid = skipSpaces(arguments);
    if (!wifiCredentials_.setSsid(ssid)) {
      return result(ConfigCommand::WIFI_SSID, CommandStatus::INVALID_VALUE);
    }
    return result(ConfigCommand::WIFI_SSID, CommandStatus::OK);
  }
  if (nextToken(arguments) != nullptr) {
    return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
  }
  if (strcmp(operation, "status") == 0) {
    return result(ConfigCommand::WIFI_STATUS, CommandStatus::OK);
  }
  if (strcmp(operation, "ap") == 0) {
    return result(ConfigCommand::WIFI_AP_INFO, CommandStatus::OK);
  }
  if (strcmp(operation, "scan") == 0) {
    return result(ConfigCommand::WIFI_SCAN, CommandStatus::OK);
  }
  if (strcmp(operation, "password") == 0) {
    return result(ConfigCommand::WIFI_PASSWORD, CommandStatus::OK);
  }
  if (strcmp(operation, "connect") == 0) {
    return result(ConfigCommand::WIFI_CONNECT, CommandStatus::OK);
  }
  if (strcmp(operation, "disconnect") == 0) {
    return result(ConfigCommand::WIFI_DISCONNECT, CommandStatus::OK);
  }
  return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
}

CommandResult ConfigCommandParser::executeWspr(char *arguments) {
  char *operation = nextToken(arguments);
  if (operation == nullptr) {
    return result(ConfigCommand::NONE, CommandStatus::MISSING_ARGUMENT);
  }
  if (strcmp(operation, "show") == 0 && nextToken(arguments) == nullptr) {
    return result(ConfigCommand::WSPR_SHOW, CommandStatus::OK);
  }
  if (strcmp(operation, "frequency") == 0) {
    char *modeKeyword = nextToken(arguments);
    char *mode = nextToken(arguments);
    if (modeKeyword == nullptr || mode == nullptr ||
        nextToken(arguments) != nullptr || strcmp(modeKeyword, "mode") != 0) {
      return result(ConfigCommand::WSPR_FREQUENCY_MODE,
                    CommandStatus::MISSING_ARGUMENT);
    }
    if (strcmp(mode, "fixed") == 0) {
      candidate_.frequencyMode = WsprFrequencyMode::FIXED;
      return frequencyModeResult(WsprFrequencyMode::FIXED);
    }
    if (strcmp(mode, "random") == 0) {
      candidate_.frequencyMode = WsprFrequencyMode::RANDOM;
      return frequencyModeResult(WsprFrequencyMode::RANDOM);
    }
    return result(ConfigCommand::WSPR_FREQUENCY_MODE,
                  CommandStatus::INVALID_VALUE);
  }
  if (strcmp(operation, "set") == 0) return setValue(arguments);
  return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
}

CommandResult ConfigCommandParser::executeConfig(char *arguments) {
  char *operation = nextToken(arguments);
  if (operation == nullptr) {
    return result(ConfigCommand::CONFIG_ENTER, CommandStatus::OK);
  }
  if (nextToken(arguments) != nullptr) {
    return result(ConfigCommand::NONE, CommandStatus::MISSING_ARGUMENT);
  }
  if (strcmp(operation, "exit") == 0) {
    return result(ConfigCommand::CONFIG_EXIT, CommandStatus::OK);
  }
  if (strcmp(operation, "show") == 0) {
    return result(ConfigCommand::CONFIG_SHOW, CommandStatus::OK);
  }
  if (strcmp(operation, "discard") == 0) {
    candidate_ = saved_;
    wifiCredentials_ = savedWifi_;
    return result(ConfigCommand::CONFIG_DISCARD, CommandStatus::OK);
  }
  if (strcmp(operation, "defaults") == 0) {
    candidate_ = BeaconSettings{};
    wifiCredentials_ = WifiCredentials{};
    return result(ConfigCommand::CONFIG_DEFAULTS, CommandStatus::OK);
  }
  if (strcmp(operation, "save") == 0) {
    return result(ConfigCommand::CONFIG_SAVE, CommandStatus::OK);
  }
  return result(ConfigCommand::NONE, CommandStatus::UNKNOWN_COMMAND);
}

CommandResult ConfigCommandParser::setValue(char *arguments) {
  char *name = nextToken(arguments);
  char *value = nextToken(arguments);
  if (name == nullptr || value == nullptr || nextToken(arguments) != nullptr) {
    return result(ConfigCommand::WSPR_SET, CommandStatus::MISSING_ARGUMENT);
  }

  BeaconSettings proposed = candidate_;
  ConfigField field = ConfigField::CALLSIGN;
  ConfigValidation validation = ConfigValidation::INVALID_FORMAT;
  long number = 0;

  if (strcmp(name, "callsign") == 0) {
    uppercase(value);
    field = ConfigField::CALLSIGN;
    validation = ConfigValidator::validateCallsign(value);
    if (validation == ConfigValidation::VALID) strcpy(proposed.callsign, value);
  } else if (strcmp(name, "locator") == 0) {
    uppercase(value);
    field = ConfigField::LOCATOR;
    validation = ConfigValidator::validateLocator(value);
    if (validation == ConfigValidation::VALID) strcpy(proposed.fixedLocator, value);
  } else if (strcmp(name, "band") == 0) {
    field = ConfigField::BAND;
    const BandPlanEntry *selected = nullptr;
    for (uint8_t index = 0; index < BandPlan::size(); ++index) {
      const BandPlanEntry *band = BandPlan::at(index);
      if (band != nullptr && strcmp(value, band->name) == 0) {
        selected = band;
        break;
      }
    }
    if (selected != nullptr && selected->txEnabled) {
      proposed.band = selected->id;
      proposed.baseFrequencyHz = selected->nominalToneZeroHz;
      validation = validateCandidate(proposed);
    }
  } else if (strcmp(name, "power") == 0) {
    field = ConfigField::POWER_DBM;
    if (parseLong(value, number) && number >= 0 && number <= UINT8_MAX) {
      validation = ConfigValidator::validatePower(static_cast<uint8_t>(number));
      if (validation == ConfigValidation::VALID) proposed.powerDbm = number;
    }
  } else if (strcmp(name, "interval") == 0) {
    field = ConfigField::INTERVAL_MINUTES;
    if (parseLong(value, number) && number >= 0 && number <= UINT8_MAX) {
      validation = ConfigValidator::validateInterval(static_cast<uint8_t>(number));
      if (validation == ConfigValidation::VALID) proposed.transmitIntervalMinutes = number;
    }
  } else if (strcmp(name, "frequency") == 0) {
    field = ConfigField::BASE_FREQUENCY_HZ;
    if (parseLong(value, number) && number >= 0) {
      validation = ConfigValidator::validateBaseFrequency(proposed.band,
                                                          number);
      if (validation == ConfigValidation::VALID) proposed.baseFrequencyHz = number;
    }
  } else if (strcmp(name, "offset") == 0) {
    field = ConfigField::FREQUENCY_OFFSET_HZ;
    if (parseLong(value, number)) {
      validation = ConfigValidator::validateOffset(number);
      if (validation == ConfigValidation::VALID) proposed.frequencyOffsetHz = number;
    }
  } else if (strcmp(name, "correction") == 0) {
    field = ConfigField::SI5351_CORRECTION;
    if (parseLong(value, number)) {
      validation = ConfigValidator::validateCorrection(number);
      if (validation == ConfigValidation::VALID) proposed.si5351Correction = number;
    }
  } else if (strcmp(name, "power-save") == 0) {
    field = ConfigField::POWER_SAVE;
    if (strcmp(value, "on") == 0 || strcmp(value, "off") == 0) {
      proposed.powerSave = strcmp(value, "on") == 0;
      validation = validateCandidate(proposed);
    }
  } else {
    return result(ConfigCommand::WSPR_SET, CommandStatus::UNKNOWN_COMMAND);
  }

  if (validation != ConfigValidation::VALID ||
      validateCandidate(proposed) != ConfigValidation::VALID) {
    return result(ConfigCommand::WSPR_SET, CommandStatus::INVALID_VALUE, field);
  }
  candidate_ = proposed;
  return result(ConfigCommand::WSPR_SET, CommandStatus::OK, field);
}
