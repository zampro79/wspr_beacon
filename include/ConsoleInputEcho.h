#pragma once

#include <stdint.h>

enum class ConsoleEchoAction : uint8_t {
  NONE,
  CHARACTER,
  ERASE,
  NEWLINE
};

struct ConsoleEchoResult {
  ConsoleEchoAction action = ConsoleEchoAction::NONE;
  char character = '\0';
};

class ConsoleInputEcho {
 public:
  void setSuspended(bool suspended) { suspended_ = suspended; }
  ConsoleEchoResult accept(char value, bool secret, bool canErase,
                           bool canAppend);

 private:
  bool previousWasTerminator_ = false;
  bool suspended_ = false;
};
