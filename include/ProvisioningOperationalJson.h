#pragma once

#include <stddef.h>

class ProvisioningOperationalJson {
 public:
  static bool valid(const char *json, size_t length);
};
