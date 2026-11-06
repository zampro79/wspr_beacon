#include "ConsoleInputEcho.h"

ConsoleEchoResult ConsoleInputEcho::accept(char value, bool secret,
                                           bool canErase, bool canAppend) {
  ConsoleEchoResult result;
  if (suspended_) return result;
  if (value == '\r' || value == '\n') {
    if (previousWasTerminator_) return result;
    previousWasTerminator_ = true;
    result.action = ConsoleEchoAction::NEWLINE;
    return result;
  }
  previousWasTerminator_ = false;
  if (secret) return result;
  if (value == '\b' || static_cast<uint8_t>(value) == 0x7fU) {
    if (canErase) result.action = ConsoleEchoAction::ERASE;
    return result;
  }
  if (canAppend && static_cast<uint8_t>(value) >= 0x20U) {
    result.action = ConsoleEchoAction::CHARACTER;
    result.character = value;
  }
  return result;
}
