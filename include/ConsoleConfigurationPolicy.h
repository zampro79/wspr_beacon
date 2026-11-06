#pragma once

#include "ConfigCommandParser.h"

class ConsoleConfigurationPolicy {
 public:
  static bool requiresOwnership(ConfigCommand command) {
    return command == ConfigCommand::WSPR_SET ||
           command == ConfigCommand::WSPR_FREQUENCY_MODE ||
           command == ConfigCommand::CONFIG_SAVE ||
           command == ConfigCommand::CONFIG_DISCARD ||
           command == ConfigCommand::CONFIG_DEFAULTS ||
           command == ConfigCommand::FACTORY_RESET;
  }

  static bool allowed(ConfigCommand command, bool cliOwnerActive) {
    return cliOwnerActive || !requiresOwnership(command);
  }
};
