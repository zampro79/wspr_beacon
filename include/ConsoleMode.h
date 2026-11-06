#pragma once

#include <stdint.h>

enum class ConsoleMode : uint8_t { MONITOR, INTERACTIVE };

class ConsoleModeController {
 public:
  void noteInput(char value, bool timingSafe = true);
  void setMonitorEnabled(bool enabled);
  ConsoleMode mode() const { return mode_; }
  bool periodicDiagnosticsEnabled() const {
    return mode_ == ConsoleMode::MONITOR;
  }
  bool promptAllowed(bool timingSafe, bool inputPending) const {
    return mode_ == ConsoleMode::INTERACTIVE && timingSafe && !inputPending;
  }

 private:
  ConsoleMode mode_ = ConsoleMode::INTERACTIVE;
};
