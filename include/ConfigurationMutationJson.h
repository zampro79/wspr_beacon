#pragma once

#include <stddef.h>
#include <stdint.h>

#include "BeaconSettings.h"

enum class ConfigurationMutationParseResult : uint8_t {
  OK,
  MALFORMED,
  UNKNOWN_FIELD,
  DUPLICATE_FIELD,
  WRONG_TYPE,
  MISSING_FIELD,
  OUT_OF_RANGE
};

struct ConfigurationCandidateRequest {
  BeaconSettings settings;
  uint32_t expectedRevision = 0;
};

struct ConfigurationSaveRequest {
  uint32_t expectedRevision = 0;
};

class ConfigurationMutationJson {
 public:
  static ConfigurationMutationParseResult parseCandidate(
      const char *json, size_t length, ConfigurationCandidateRequest &request);
  static ConfigurationMutationParseResult parseRevision(
      const char *json, size_t length, uint32_t &expectedRevision);
  static ConfigurationMutationParseResult parseSave(
      const char *json, size_t length, ConfigurationSaveRequest &request);
};
