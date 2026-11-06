#pragma once

#include <stdint.h>
#include <string.h>

#include "ConfigurationSessionController.h"
#include "IdleStatusScreen.h"

struct ConfigurationStatusSnapshot {
  static constexpr uint8_t LINE_COUNT = 3;
  static constexpr uint8_t MAX_CHARACTERS = 21;
  char lines[LINE_COUNT][MAX_CHARACTERS + 1] = {};
};

class ConfigurationStatusScreen {
 public:
  static bool visible(const ConfigurationSessionSnapshot &session,
                      OledRfState rfState) {
    return rfState == OledRfState::READY &&
           (session.mode == FirmwareOperatingMode::CONFIGURATION ||
            session.resumeGuardActive);
  }

  static ConfigurationStatusSnapshot build(
      const ConfigurationSessionSnapshot &session) {
    ConfigurationStatusSnapshot result;
    if (session.mode == FirmwareOperatingMode::CONFIGURATION) {
      strcpy(result.lines[0], "CONFIGURATION");
      strcpy(result.lines[1], "TX DISABLED");
      const char *owner = session.cliActive && session.webActive
          ? "BOTH" : (session.cliActive ? "CLI" : "WEB");
      strcpy(result.lines[2], "SESSION: ");
      strcat(result.lines[2], owner);
    } else if (session.resumeGuardActive) {
      strcpy(result.lines[0], "TX RESUME GUARD");
      strcpy(result.lines[1], "TX DISABLED");
      strcpy(result.lines[2], "WAITING SAFE SLOT");
    }
    return result;
  }
};

static_assert(sizeof(ConfigurationStatusSnapshot) == 66,
              "Configuration OLED snapshot size changed");
