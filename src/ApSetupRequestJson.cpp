#include "ApSetupRequestJson.h"

#include <limits.h>
#include <string.h>

namespace {
class Parser {
 public:
  Parser(const char *data, size_t length) : cursor_(data), end_(data + length) {}
  void space() {
    while (cursor_ < end_ && (*cursor_ == ' ' || *cursor_ == '\t' ||
                              *cursor_ == '\r' || *cursor_ == '\n')) ++cursor_;
  }
  bool take(char value) {
    space();
    if (cursor_ == end_ || *cursor_ != value) return false;
    ++cursor_;
    return true;
  }
  bool done() { space(); return cursor_ == end_; }
  bool string(char *output, size_t capacity) {
    space();
    if (cursor_ == end_ || *cursor_++ != '"' || capacity == 0) return false;
    size_t used = 0;
    while (cursor_ < end_ && *cursor_ != '"') {
      const unsigned char value = static_cast<unsigned char>(*cursor_++);
      if (value < 0x20 || value == '\\' || used + 1 >= capacity) return false;
      output[used++] = static_cast<char>(value);
    }
    if (cursor_ == end_ || *cursor_++ != '"') return false;
    output[used] = '\0';
    return true;
  }
  bool number(uint32_t &output) {
    space();
    if (cursor_ == end_ || *cursor_ < '0' || *cursor_ > '9') return false;
    if (*cursor_ == '0' && cursor_ + 1 < end_ && cursor_[1] >= '0' &&
        cursor_[1] <= '9') return false;
    uint64_t value = 0;
    while (cursor_ < end_ && *cursor_ >= '0' && *cursor_ <= '9') {
      value = value * 10U + static_cast<unsigned>(*cursor_++ - '0');
      if (value > UINT32_MAX) return false;
    }
    output = static_cast<uint32_t>(value);
    return true;
  }
 private:
  const char *cursor_;
  const char *end_;
};
}  // namespace

ApSetupParseResult ApSetupRequestJson::parse(const char *json, size_t length,
                                              ApSetupRequest &request) {
  request = ApSetupRequest();
  if (json == nullptr) return ApSetupParseResult::MALFORMED;
  Parser parser(json, length);
  if (!parser.take('{')) return ApSetupParseResult::MALFORMED;
  unsigned seen = 0;
  bool first = true;
  while (true) {
    parser.space();
    if (parser.take('}')) break;
    if (!first && !parser.take(',')) return ApSetupParseResult::MALFORMED;
    first = false;
    char key[24];
    if (!parser.string(key, sizeof(key)) || !parser.take(':'))
      return ApSetupParseResult::MALFORMED;
    const unsigned field = strcmp(key, "expected_revision") == 0 ? 1U :
                           strcmp(key, "confirm") == 0 ? 2U : 0U;
    if (field == 0) return ApSetupParseResult::UNKNOWN_FIELD;
    if (seen & field) return ApSetupParseResult::DUPLICATE_FIELD;
    seen |= field;
    if (field == 1) {
      if (!parser.number(request.expectedRevision))
        return ApSetupParseResult::WRONG_TYPE;
    } else {
      char confirmation[24];
      if (!parser.string(confirmation, sizeof(confirmation)))
        return ApSetupParseResult::WRONG_TYPE;
      if (strcmp(confirmation, "enter_wifi_setup") != 0)
        return ApSetupParseResult::OUT_OF_RANGE;
    }
  }
  if (!parser.done()) return ApSetupParseResult::MALFORMED;
  return seen == 3U ? ApSetupParseResult::OK
                    : ApSetupParseResult::MISSING_FIELD;
}
