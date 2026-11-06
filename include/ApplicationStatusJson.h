#pragma once

#include <stddef.h>

#include "ApplicationStatusSnapshot.h"

class ApplicationStatusJson {
 public:
  static constexpr size_t MAX_RESPONSE_BYTES = 2304;

  static bool serialize(const ApplicationStatusSnapshot &snapshot,
                        char *destination, size_t capacity, size_t &length);
};
