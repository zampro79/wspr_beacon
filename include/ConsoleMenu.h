#pragma once

#include <stdint.h>

#include "ConfigCommandParser.h"

enum class ConsoleMenuScreen : uint8_t {
  INACTIVE,
  MAIN,
  WSPR,
  WSPR_FREQUENCY_MODE,
  WIFI,
  CALIBRATION,
  WSPR_VALUE,
  WIFI_SSID,
  WIFI_PASSWORD,
  CALIBRATION_VALUE
};

enum class ConsoleMenuResultKind : uint8_t {
  NAVIGATED,
  COMMAND,
  VALUE_ACCEPTED,
  INVALID_SELECTION,
  INVALID_VALUE,
  SHOW_GPS_TIME,
  EXITED
};

struct ConsoleMenuResult {
  ConsoleMenuResultKind kind = ConsoleMenuResultKind::NAVIGATED;
  ConfigCommand command = ConfigCommand::NONE;
  CommandStatus status = CommandStatus::OK;
  WsprFrequencyMode frequencyMode = WsprFrequencyMode::FIXED;
};

class ConsoleMenu {
 public:
  void enter();
  void exit() { screen_ = ConsoleMenuScreen::INACTIVE; }
  bool active() const { return screen_ != ConsoleMenuScreen::INACTIVE; }
  bool passwordExpected() const {
    return screen_ == ConsoleMenuScreen::WIFI_PASSWORD;
  }
  ConsoleMenuScreen screen() const { return screen_; }
  ConfigField pendingField() const { return pendingField_; }
  ConsoleMenuResult accept(char *line, ConfigCommandParser &parser);

 private:
  ConsoleMenuScreen screen_ = ConsoleMenuScreen::INACTIVE;
  ConfigField pendingField_ = ConfigField::CALLSIGN;

  ConsoleMenuResult acceptMain(const char *line);
  ConsoleMenuResult acceptWspr(const char *line);
  ConsoleMenuResult acceptFrequencyMode(char *line,
                                        ConfigCommandParser &parser);
  ConsoleMenuResult acceptWifi(const char *line);
  ConsoleMenuResult acceptCalibration(const char *line);
  ConsoleMenuResult acceptValue(char *line, ConfigCommandParser &parser);
  static bool selection(const char *line, uint8_t &value);
  static ConsoleMenuResult result(ConsoleMenuResultKind kind,
                                  ConfigCommand command = ConfigCommand::NONE,
                                  CommandStatus status = CommandStatus::OK,
                                  WsprFrequencyMode frequencyMode =
                                      WsprFrequencyMode::FIXED);
};
