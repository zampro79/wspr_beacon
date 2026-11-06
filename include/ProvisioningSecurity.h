#pragma once

#include <stddef.h>
#include <stdint.h>

class ProvisioningSecurity {
 public:
  using FillRandom = bool (*)(void *context, uint8_t *destination,
                              size_t length);
  static constexpr size_t PASSWORD_DIGITS = 8;

  static bool generateNumericPassword(FillRandom fill, void *context,
                                      char destination[PASSWORD_DIGITS + 1U]);
  static void clear(void *value, size_t length);
};
