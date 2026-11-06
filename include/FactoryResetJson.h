#pragma once
#include <stddef.h>
#include <stdint.h>

enum class FactoryResetParseResult : uint8_t {
  OK, MALFORMED, UNKNOWN_FIELD, DUPLICATE_FIELD, WRONG_TYPE,
  MISSING_FIELD, OUT_OF_RANGE
};
struct FactoryResetRequest { uint32_t expectedRevision = 0; };
class FactoryResetJson {
 public:
  static FactoryResetParseResult parse(const char *json, size_t length,
                                       FactoryResetRequest &request);
};
