#pragma once

#include <stddef.h>
#include <stdint.h>

enum class ApSetupParseResult : uint8_t {
  OK, MALFORMED, UNKNOWN_FIELD, DUPLICATE_FIELD, WRONG_TYPE,
  MISSING_FIELD, OUT_OF_RANGE
};

struct ApSetupRequest { uint32_t expectedRevision = 0; };

class ApSetupRequestJson {
 public:
  static ApSetupParseResult parse(const char *json, size_t length,
                                  ApSetupRequest &request);
};
