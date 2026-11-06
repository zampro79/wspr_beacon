#include "ProvisioningOperationalJson.h"

#include <string.h>

namespace {
void skipSpaces(const char *&cursor, const char *end) {
  while (cursor < end && (*cursor == ' ' || *cursor == '\t' ||
                          *cursor == '\r' || *cursor == '\n')) {
    ++cursor;
  }
}

bool take(const char *&cursor, const char *end, char value) {
  skipSpaces(cursor, end);
  if (cursor >= end || *cursor != value) return false;
  ++cursor;
  return true;
}

bool literal(const char *&cursor, const char *end, const char *value) {
  skipSpaces(cursor, end);
  const size_t length = strlen(value);
  if (static_cast<size_t>(end - cursor) < length ||
      memcmp(cursor, value, length) != 0) return false;
  cursor += length;
  return true;
}
}  // namespace

bool ProvisioningOperationalJson::valid(const char *json, size_t length) {
  if (json == nullptr) return false;
  const char *cursor = json;
  const char *end = json + length;
  if (!take(cursor, end, '{') ||
      !literal(cursor, end, "\"confirm\"") ||
      !take(cursor, end, ':') ||
      !literal(cursor, end, "\"ap_operational\"") ||
      !take(cursor, end, '}')) return false;
  skipSpaces(cursor, end);
  return cursor == end;
}
