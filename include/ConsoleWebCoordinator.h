#pragma once

class ConsoleWebCoordinator {
 public:
  static bool periodicReportAllowed(bool monitorEnabled,
                                    bool httpClientWasActive,
                                    bool httpClientIsActive) {
    return monitorEnabled && !httpClientWasActive && !httpClientIsActive;
  }
};
