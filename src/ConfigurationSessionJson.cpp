#include "ConfigurationSessionJson.h"

#include <stdio.h>
#include <string.h>

namespace {
class Parser {
 public:
  Parser(const char *data, size_t length) : cursor_(data), end_(data + length) {}
  void spaces() { while (cursor_ < end_ && (*cursor_ == ' ' || *cursor_ == '\t' || *cursor_ == '\r' || *cursor_ == '\n')) ++cursor_; }
  bool take(char value) { spaces(); if (cursor_ >= end_ || *cursor_ != value) return false; ++cursor_; return true; }
  bool finished() { spaces(); return cursor_ == end_; }
  bool string(char *out, size_t capacity) {
    spaces();
    if (cursor_ >= end_ || *cursor_++ != '"' || capacity == 0) return false;
    size_t length = 0;
    while (cursor_ < end_ && *cursor_ != '"') {
      const char value = *cursor_++;
      if (value == '\\' || static_cast<unsigned char>(value) < 0x20U ||
          length + 1U >= capacity) return false;
      out[length++] = value;
    }
    if (cursor_ >= end_ || *cursor_++ != '"') return false;
    out[length] = '\0';
    return true;
  }
  bool unsignedInteger(uint32_t &value) {
    spaces();
    if (cursor_ >= end_ || *cursor_ < '0' || *cursor_ > '9') return false;
    if (*cursor_ == '0' && cursor_ + 1 < end_ && cursor_[1] >= '0' && cursor_[1] <= '9') return false;
    uint64_t result = 0;
    while (cursor_ < end_ && *cursor_ >= '0' && *cursor_ <= '9') {
      result = result * 10U + static_cast<uint8_t>(*cursor_++ - '0');
      if (result > UINT32_MAX) return false;
    }
    value = static_cast<uint32_t>(result);
    return true;
  }
 private:
  const char *cursor_;
  const char *end_;
};

ConfigurationSessionParseResult parse(const char *json, size_t length,
                                      ConfigurationSessionEnterRequest &request,
                                      bool includeRevision) {
  if (json == nullptr) return ConfigurationSessionParseResult::MALFORMED;
  Parser parser(json, length);
  if (!parser.take('{')) return ConfigurationSessionParseResult::MALFORMED;
  uint8_t seen = 0;
  bool first = true;
  while (true) {
    parser.spaces();
    if (parser.take('}')) break;
    if (!first && !parser.take(',')) return ConfigurationSessionParseResult::MALFORMED;
    first = false;
    char key[24];
    if (!parser.string(key, sizeof(key)) || !parser.take(':')) return ConfigurationSessionParseResult::MALFORMED;
    uint8_t field = 0;
    if (strcmp(key, "session_id") == 0) field = 1U;
    else if (strcmp(key, "expected_revision") == 0 && includeRevision) field = 2U;
    else return ConfigurationSessionParseResult::UNKNOWN_FIELD;
    if ((seen & field) != 0) return ConfigurationSessionParseResult::DUPLICATE_FIELD;
    seen |= field;
    if (field == 1U) {
      if (!parser.string(request.sessionId, sizeof(request.sessionId))) return ConfigurationSessionParseResult::WRONG_TYPE;
      if (!ConfigurationSessionController::validWebSessionId(request.sessionId)) return ConfigurationSessionParseResult::OUT_OF_RANGE;
    } else if (!parser.unsignedInteger(request.expectedRevision)) {
      return ConfigurationSessionParseResult::WRONG_TYPE;
    }
  }
  if (!parser.finished()) return ConfigurationSessionParseResult::MALFORMED;
  return seen == (includeRevision ? 3U : 1U)
             ? ConfigurationSessionParseResult::OK
             : ConfigurationSessionParseResult::MISSING_FIELD;
}
}  // namespace

ConfigurationSessionParseResult ConfigurationSessionJson::parseEnter(
    const char *json, size_t length, ConfigurationSessionEnterRequest &request) {
  request = ConfigurationSessionEnterRequest();
  return parse(json, length, request, true);
}

ConfigurationSessionParseResult ConfigurationSessionJson::parseOwner(
    const char *json, size_t length,
    char (&sessionId)[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH + 1U]) {
  ConfigurationSessionEnterRequest request;
  const ConfigurationSessionParseResult result = parse(json, length, request, false);
  memcpy(sessionId, request.sessionId, sizeof(request.sessionId));
  return result;
}

bool ConfigurationSessionJson::serialize(
    const ConfigurationSessionSnapshot &snapshot, char *destination,
    size_t capacity, size_t &length) {
  const int written = snprintf(
      destination, capacity,
      "{\"schema\":1,\"mode\":\"%s\",\"revision\":%lu,"
      "\"cli_active\":%s,\"web_active\":%s,"
      "\"web_expiry_remaining_ms\":%lu,\"resume_guard\":%s,"
      "\"tx_admission_allowed\":%s}\n",
      snapshot.mode == FirmwareOperatingMode::CONFIGURATION ? "CONFIGURATION" : "NORMAL",
      static_cast<unsigned long>(snapshot.revision),
      snapshot.cliActive ? "true" : "false",
      snapshot.webActive ? "true" : "false",
      static_cast<unsigned long>(snapshot.webRemainingMillis),
      snapshot.resumeGuardActive ? "true" : "false",
      snapshot.txAdmissionAllowed ? "true" : "false");
  length = written > 0 && static_cast<size_t>(written) < capacity
               ? static_cast<size_t>(written) : 0;
  return length != 0;
}
