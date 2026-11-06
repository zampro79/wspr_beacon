#pragma once

#include <stddef.h>

class FirmwareIdentity {
 public:
  static constexpr const char *project() { return "WSPR Beacon 2.0"; }
  static constexpr const char *version() { return "dev"; }
  static const char *profile();
  static const char *target();
  static const char *commit();

  static bool format(char *destination, size_t capacity);
};
