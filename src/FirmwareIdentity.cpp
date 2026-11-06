#include "FirmwareIdentity.h"

#include <stdio.h>

#include "BuildProfile.h"

#ifndef WSPR_SOURCE_COMMIT
  #define WSPR_SOURCE_COMMIT "unknown"
#endif

const char *FirmwareIdentity::profile() { return WSPR_BUILD_PROFILE_NAME; }

const char *FirmwareIdentity::target() {
#if defined(ARDUINO_ARCH_ESP8266)
  return "ESP8266";
#elif defined(ARDUINO_ARCH_ESP32)
  return "ESP32";
#elif defined(ARDUINO_ARCH_MEGAAVR)
  return "NANO_EVERY";
#else
  return "NATIVE";
#endif
}

const char *FirmwareIdentity::commit() { return WSPR_SOURCE_COMMIT; }

bool FirmwareIdentity::format(char *destination, size_t capacity) {
  if (destination == nullptr || capacity == 0U) return false;
  const int written = snprintf(destination, capacity,
      "%s | %s | %s | %s | commit %s", project(), profile(), target(),
      version(), commit());
  if (written < 0 || static_cast<size_t>(written) >= capacity) {
    destination[0] = '\0';
    return false;
  }
  return true;
}
