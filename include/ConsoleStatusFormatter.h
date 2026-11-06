#pragma once

#include "ConsoleStatus.h"

class ConsoleStatusFormatter {
 public:
  using LineWriter = void (*)(void *context, const char *line);

  static void writeSummary(const ConsoleStatusSnapshot &snapshot,
                           const char *boardName, LineWriter writer,
                           void *context);
  static bool outputAllowed(ConsoleTxState state);
};
