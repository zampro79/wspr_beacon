#include "ConsoleMode.h"

void ConsoleModeController::noteInput(char value, bool timingSafe) {
  if (timingSafe && value != '\r' && value != '\n') {
    mode_ = ConsoleMode::INTERACTIVE;
  }
}

void ConsoleModeController::setMonitorEnabled(bool enabled) {
  mode_ = enabled ? ConsoleMode::MONITOR : ConsoleMode::INTERACTIVE;
}
