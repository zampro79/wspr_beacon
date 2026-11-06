#include "ConsoleMenu.h"

#include <stdio.h>
#include <string.h>

namespace {
const char *fieldName(ConfigField field) {
  switch (field) {
    case ConfigField::CALLSIGN: return "callsign";
    case ConfigField::LOCATOR: return "locator";
    case ConfigField::BAND: return "band";
    case ConfigField::POWER_DBM: return "power";
    case ConfigField::INTERVAL_MINUTES: return "interval";
    case ConfigField::BASE_FREQUENCY_HZ: return "frequency";
    case ConfigField::FREQUENCY_OFFSET_HZ: return "offset";
    case ConfigField::SI5351_CORRECTION: return "correction";
    case ConfigField::POWER_SAVE: return "power-save";
  }
  return "";
}
}  // namespace

void ConsoleMenu::enter() {
  screen_ = ConsoleMenuScreen::MAIN;
}

ConsoleMenuResult ConsoleMenu::result(ConsoleMenuResultKind kind,
                                      ConfigCommand command,
                                      CommandStatus status,
                                      WsprFrequencyMode frequencyMode) {
  ConsoleMenuResult response;
  response.kind = kind;
  response.command = command;
  response.status = status;
  response.frequencyMode = frequencyMode;
  return response;
}

bool ConsoleMenu::selection(const char *line, uint8_t &value) {
  if (line == nullptr || line[0] < '0' || line[0] > '9' || line[1] != '\0') {
    return false;
  }
  value = static_cast<uint8_t>(line[0] - '0');
  return true;
}

ConsoleMenuResult ConsoleMenu::accept(char *line, ConfigCommandParser &parser) {
  switch (screen_) {
    case ConsoleMenuScreen::MAIN: return acceptMain(line);
    case ConsoleMenuScreen::WSPR: return acceptWspr(line);
    case ConsoleMenuScreen::WSPR_FREQUENCY_MODE:
      return acceptFrequencyMode(line, parser);
    case ConsoleMenuScreen::WIFI: return acceptWifi(line);
    case ConsoleMenuScreen::CALIBRATION: return acceptCalibration(line);
    case ConsoleMenuScreen::WSPR_VALUE:
    case ConsoleMenuScreen::WIFI_SSID:
    case ConsoleMenuScreen::WIFI_PASSWORD:
    case ConsoleMenuScreen::CALIBRATION_VALUE:
      return acceptValue(line, parser);
    case ConsoleMenuScreen::INACTIVE:
      return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
  return result(ConsoleMenuResultKind::INVALID_SELECTION);
}

ConsoleMenuResult ConsoleMenu::acceptMain(const char *line) {
  uint8_t choice = 0;
  if (!selection(line, choice)) return result(ConsoleMenuResultKind::INVALID_SELECTION);
  switch (choice) {
    case 0:
    case 9:
      screen_ = ConsoleMenuScreen::INACTIVE;
      return result(ConsoleMenuResultKind::EXITED);
    case 1:
      screen_ = ConsoleMenuScreen::WSPR;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 2:
      screen_ = ConsoleMenuScreen::WIFI;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 3: return result(ConsoleMenuResultKind::SHOW_GPS_TIME);
    case 4:
      screen_ = ConsoleMenuScreen::CALIBRATION;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 5:
      return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::STATUS_MODULES);
    case 6: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::STATUS);
    case 7: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::CONFIG_SAVE);
    case 8: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::CONFIG_DISCARD);
    default: return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
}

ConsoleMenuResult ConsoleMenu::acceptWspr(const char *line) {
  uint8_t choice = 0;
  if (!selection(line, choice)) return result(ConsoleMenuResultKind::INVALID_SELECTION);
  if (choice == 0) {
    screen_ = ConsoleMenuScreen::MAIN;
    return result(ConsoleMenuResultKind::NAVIGATED);
  }
  switch (choice) {
    case 1: pendingField_ = ConfigField::CALLSIGN; break;
    case 2: pendingField_ = ConfigField::LOCATOR; break;
    case 3: pendingField_ = ConfigField::POWER_DBM; break;
    case 4: pendingField_ = ConfigField::BAND; break;
    case 5: pendingField_ = ConfigField::BASE_FREQUENCY_HZ; break;
    case 6: pendingField_ = ConfigField::FREQUENCY_OFFSET_HZ; break;
    case 7: pendingField_ = ConfigField::INTERVAL_MINUTES; break;
    case 8:
      screen_ = ConsoleMenuScreen::WSPR_FREQUENCY_MODE;
      return result(ConsoleMenuResultKind::NAVIGATED);
    default: return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
  screen_ = ConsoleMenuScreen::WSPR_VALUE;
  return result(ConsoleMenuResultKind::NAVIGATED);
}

ConsoleMenuResult ConsoleMenu::acceptFrequencyMode(
    char *line, ConfigCommandParser &parser) {
  uint8_t choice = 0;
  if (!selection(line, choice)) {
    return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
  if (choice == 0) {
    screen_ = ConsoleMenuScreen::WSPR;
    return result(ConsoleMenuResultKind::NAVIGATED);
  }
  const char *mode = choice == 1 ? "fixed" : choice == 2 ? "random" : nullptr;
  if (mode == nullptr) {
    return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
  char command[32];
  const int written = snprintf(command, sizeof(command),
                               "wspr frequency mode %s", mode);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(command)) {
    return result(ConsoleMenuResultKind::INVALID_VALUE);
  }
  const CommandResult parsed = parser.execute(command);
  if (parsed.status != CommandStatus::OK) {
    return result(ConsoleMenuResultKind::INVALID_VALUE, parsed.command,
                  parsed.status);
  }
  screen_ = ConsoleMenuScreen::WSPR;
  return result(ConsoleMenuResultKind::COMMAND, parsed.command, parsed.status,
                parsed.frequencyMode);
}

ConsoleMenuResult ConsoleMenu::acceptWifi(const char *line) {
  uint8_t choice = 0;
  if (!selection(line, choice)) return result(ConsoleMenuResultKind::INVALID_SELECTION);
  switch (choice) {
    case 0:
      screen_ = ConsoleMenuScreen::MAIN;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 1: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::WIFI_STATUS);
    case 2:
      screen_ = ConsoleMenuScreen::WIFI_SSID;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 3:
      screen_ = ConsoleMenuScreen::WIFI_PASSWORD;
      return result(ConsoleMenuResultKind::NAVIGATED);
    case 4: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::WIFI_SCAN);
    case 5: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::WIFI_CONNECT);
    case 6: return result(ConsoleMenuResultKind::COMMAND, ConfigCommand::WIFI_DISCONNECT);
    default: return result(ConsoleMenuResultKind::INVALID_SELECTION);
  }
}

ConsoleMenuResult ConsoleMenu::acceptCalibration(const char *line) {
  uint8_t choice = 0;
  if (!selection(line, choice)) return result(ConsoleMenuResultKind::INVALID_SELECTION);
  if (choice == 0) {
    screen_ = ConsoleMenuScreen::MAIN;
    return result(ConsoleMenuResultKind::NAVIGATED);
  }
  if (choice != 1) return result(ConsoleMenuResultKind::INVALID_SELECTION);
  pendingField_ = ConfigField::SI5351_CORRECTION;
  screen_ = ConsoleMenuScreen::CALIBRATION_VALUE;
  return result(ConsoleMenuResultKind::NAVIGATED);
}

ConsoleMenuResult ConsoleMenu::acceptValue(char *line,
                                           ConfigCommandParser &parser) {
  if (line == nullptr || line[0] == '\0') {
    return result(ConsoleMenuResultKind::INVALID_VALUE);
  }
  if (screen_ == ConsoleMenuScreen::WIFI_PASSWORD) {
    const bool accepted = parser.setWifiPassword(line);
    if (accepted) screen_ = ConsoleMenuScreen::WIFI;
    return result(accepted ? ConsoleMenuResultKind::VALUE_ACCEPTED
                           : ConsoleMenuResultKind::INVALID_VALUE,
                  ConfigCommand::WIFI_PASSWORD,
                  accepted ? CommandStatus::OK : CommandStatus::INVALID_VALUE);
  }

  char command[96];
  int written = 0;
  if (screen_ == ConsoleMenuScreen::WIFI_SSID) {
    written = snprintf(command, sizeof(command), "wifi ssid %s", line);
  } else {
    written = snprintf(command, sizeof(command), "wspr set %s %s",
                       fieldName(pendingField_), line);
  }
  if (written < 0 || static_cast<size_t>(written) >= sizeof(command)) {
    return result(ConsoleMenuResultKind::INVALID_VALUE);
  }
  const CommandResult parsed = parser.execute(command);
  if (parsed.status != CommandStatus::OK) {
    return result(ConsoleMenuResultKind::INVALID_VALUE, parsed.command,
                  parsed.status);
  }
  screen_ = screen_ == ConsoleMenuScreen::WIFI_SSID
                ? ConsoleMenuScreen::WIFI
                : (screen_ == ConsoleMenuScreen::CALIBRATION_VALUE
                       ? ConsoleMenuScreen::CALIBRATION
                       : ConsoleMenuScreen::WSPR);
  return result(ConsoleMenuResultKind::VALUE_ACCEPTED, parsed.command,
                parsed.status);
}
