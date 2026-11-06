#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigurationSessionController.h"

enum class ConfigurationSessionParseResult : uint8_t {
  OK,
  MALFORMED,
  UNKNOWN_FIELD,
  DUPLICATE_FIELD,
  WRONG_TYPE,
  MISSING_FIELD,
  OUT_OF_RANGE
};

struct ConfigurationSessionEnterRequest {
  char sessionId[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH + 1U] = {};
  uint32_t expectedRevision = 0;
};

class ConfigurationSessionJson {
 public:
  static constexpr size_t MAX_RESPONSE_BYTES = 256;

  static ConfigurationSessionParseResult parseEnter(
      const char *json, size_t length, ConfigurationSessionEnterRequest &request);
  static ConfigurationSessionParseResult parseOwner(
      const char *json, size_t length,
      char (&sessionId)[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH + 1U]);
  static bool serialize(const ConfigurationSessionSnapshot &snapshot,
                        char *destination, size_t capacity, size_t &length);
};
