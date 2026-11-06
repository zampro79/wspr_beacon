#include "ConfigurationMutationJson.h"

#include <limits.h>
#include <string.h>

#include "BandPlan.h"

namespace {
class Parser {
 public:
  Parser(const char *data, size_t length) : cursor_(data), end_(data + length) {}

  void spaces() {
    while (cursor_ < end_ && (*cursor_ == ' ' || *cursor_ == '\t' ||
                              *cursor_ == '\r' || *cursor_ == '\n')) ++cursor_;
  }
  bool take(char value) {
    spaces();
    if (cursor_ >= end_ || *cursor_ != value) return false;
    ++cursor_;
    return true;
  }
  bool finished() { spaces(); return cursor_ == end_; }
  bool string(char *out, size_t capacity) {
    spaces();
    if (cursor_ >= end_ || *cursor_++ != '"' || capacity == 0) return false;
    size_t length = 0;
    while (cursor_ < end_ && *cursor_ != '"') {
      const char c = *cursor_++;
      if (c == '\\' || static_cast<unsigned char>(c) < 0x20U ||
          length + 1U >= capacity) return false;
      out[length++] = c;
    }
    if (cursor_ >= end_ || *cursor_++ != '"') return false;
    out[length] = '\0';
    return true;
  }
  bool integer(int64_t &value) {
    spaces();
    if (cursor_ >= end_) return false;
    bool negative = false;
    if (*cursor_ == '-') { negative = true; ++cursor_; }
    if (cursor_ >= end_ || *cursor_ < '0' || *cursor_ > '9') return false;
    if (*cursor_ == '0' && cursor_ + 1 < end_ && cursor_[1] >= '0' &&
        cursor_[1] <= '9') return false;
    uint64_t magnitude = 0;
    while (cursor_ < end_ && *cursor_ >= '0' && *cursor_ <= '9') {
      const uint8_t digit = static_cast<uint8_t>(*cursor_++ - '0');
      if (magnitude > (UINT64_MAX - digit) / 10U) return false;
      magnitude = magnitude * 10U + digit;
    }
    if (negative) {
      if (magnitude > static_cast<uint64_t>(INT64_MAX) + 1U) return false;
      value = magnitude == static_cast<uint64_t>(INT64_MAX) + 1U
                  ? INT64_MIN : -static_cast<int64_t>(magnitude);
    } else {
      if (magnitude > static_cast<uint64_t>(INT64_MAX)) return false;
      value = static_cast<int64_t>(magnitude);
    }
    return true;
  }
  bool boolean(bool &value) {
    spaces();
    if (static_cast<size_t>(end_ - cursor_) >= 4U &&
        memcmp(cursor_, "true", 4U) == 0) {
      cursor_ += 4;
      value = true;
      return true;
    }
    if (static_cast<size_t>(end_ - cursor_) >= 5U &&
        memcmp(cursor_, "false", 5U) == 0) {
      cursor_ += 5;
      value = false;
      return true;
    }
    return false;
  }
 private:
  const char *cursor_;
  const char *end_;
};

enum Field : uint16_t {
  CALLSIGN = 1U << 0, LOCATOR = 1U << 1, BAND = 1U << 2,
  MODE = 1U << 3, FREQUENCY = 1U << 4, OFFSET = 1U << 5,
  CORRECTION = 1U << 6, POWER = 1U << 7, INTERVAL = 1U << 8,
  POWER_SAVE = 1U << 9, REVISION = 1U << 10
};
constexpr uint16_t ALL_FIELDS = (1U << 11) - 1U;

uint16_t fieldFor(const char *key) {
  if (strcmp(key, "callsign") == 0) return CALLSIGN;
  if (strcmp(key, "locator") == 0) return LOCATOR;
  if (strcmp(key, "band") == 0) return BAND;
  if (strcmp(key, "frequency_mode") == 0) return MODE;
  if (strcmp(key, "frequency_hz") == 0) return FREQUENCY;
  if (strcmp(key, "offset_hz") == 0) return OFFSET;
  if (strcmp(key, "correction") == 0) return CORRECTION;
  if (strcmp(key, "power_dbm") == 0) return POWER;
  if (strcmp(key, "interval_min") == 0) return INTERVAL;
  if (strcmp(key, "power_save") == 0) return POWER_SAVE;
  if (strcmp(key, "expected_revision") == 0) return REVISION;
  return 0;
}

ConfigurationMutationParseResult parse(const char *json, size_t length,
                                       ConfigurationCandidateRequest &request,
                                       bool revisionOnly) {
  if (json == nullptr) return ConfigurationMutationParseResult::MALFORMED;
  Parser parser(json, length);
  if (!parser.take('{')) return ConfigurationMutationParseResult::MALFORMED;
  uint16_t seen = 0;
  bool first = true;
  while (true) {
    parser.spaces();
    if (parser.take('}')) break;
    if (!first && !parser.take(',')) return ConfigurationMutationParseResult::MALFORMED;
    first = false;
    char key[24];
    if (!parser.string(key, sizeof(key)) || !parser.take(':'))
      return ConfigurationMutationParseResult::MALFORMED;
    const uint16_t field = fieldFor(key);
    if (field == 0 || (revisionOnly && field != REVISION))
      return ConfigurationMutationParseResult::UNKNOWN_FIELD;
    if ((seen & field) != 0) return ConfigurationMutationParseResult::DUPLICATE_FIELD;
    seen |= field;
    char text[12];
    int64_t number = 0;
    switch (field) {
      case CALLSIGN:
        if (!parser.string(request.settings.callsign, sizeof(request.settings.callsign)))
          return ConfigurationMutationParseResult::WRONG_TYPE;
        break;
      case LOCATOR:
        if (!parser.string(request.settings.fixedLocator, sizeof(request.settings.fixedLocator)))
          return ConfigurationMutationParseResult::WRONG_TYPE;
        break;
      case BAND:
        if (!parser.string(text, sizeof(text))) return ConfigurationMutationParseResult::WRONG_TYPE;
        {
          const BandPlanEntry *selected = nullptr;
          for (uint8_t index = 0; index < BandPlan::size(); ++index) {
            const BandPlanEntry *band = BandPlan::at(index);
            if (band != nullptr && strcmp(text, band->name) == 0) {
              selected = band;
              break;
            }
          }
          if (selected == nullptr || !selected->txEnabled) {
            return ConfigurationMutationParseResult::OUT_OF_RANGE;
          }
          request.settings.band = selected->id;
        }
        break;
      case MODE:
        if (!parser.string(text, sizeof(text))) return ConfigurationMutationParseResult::WRONG_TYPE;
        if (strcmp(text, "FIXED") == 0) request.settings.frequencyMode = WsprFrequencyMode::FIXED;
        else if (strcmp(text, "RANDOM") == 0) request.settings.frequencyMode = WsprFrequencyMode::RANDOM;
        else return ConfigurationMutationParseResult::OUT_OF_RANGE;
        break;
      case POWER_SAVE:
        if (!parser.boolean(request.settings.powerSave))
          return ConfigurationMutationParseResult::WRONG_TYPE;
        break;
      default:
        if (!parser.integer(number)) return ConfigurationMutationParseResult::WRONG_TYPE;
        if (field == FREQUENCY) {
          if (number < 0 || number > UINT32_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.settings.baseFrequencyHz = static_cast<uint32_t>(number);
        } else if (field == OFFSET) {
          if (number < INT16_MIN || number > INT16_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.settings.frequencyOffsetHz = static_cast<int16_t>(number);
        } else if (field == CORRECTION) {
          if (number < INT32_MIN || number > INT32_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.settings.si5351Correction = static_cast<int32_t>(number);
        } else if (field == POWER) {
          if (number < 0 || number > UINT8_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.settings.powerDbm = static_cast<uint8_t>(number);
        } else if (field == INTERVAL) {
          if (number < 0 || number > UINT8_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.settings.transmitIntervalMinutes = static_cast<uint8_t>(number);
        } else if (field == REVISION) {
          if (number < 0 || number > UINT32_MAX) return ConfigurationMutationParseResult::OUT_OF_RANGE;
          request.expectedRevision = static_cast<uint32_t>(number);
        }
    }
  }
  if (!parser.finished()) return ConfigurationMutationParseResult::MALFORMED;
  const uint16_t required = revisionOnly ? REVISION : ALL_FIELDS;
  return seen == required ? ConfigurationMutationParseResult::OK
                          : ConfigurationMutationParseResult::MISSING_FIELD;
}
}  // namespace

ConfigurationMutationParseResult ConfigurationMutationJson::parseCandidate(
    const char *json, size_t length, ConfigurationCandidateRequest &request) {
  request = ConfigurationCandidateRequest();
  return parse(json, length, request, false);
}

ConfigurationMutationParseResult ConfigurationMutationJson::parseRevision(
    const char *json, size_t length, uint32_t &expectedRevision) {
  ConfigurationCandidateRequest request;
  const ConfigurationMutationParseResult result = parse(json, length, request, true);
  expectedRevision = request.expectedRevision;
  return result;
}

ConfigurationMutationParseResult ConfigurationMutationJson::parseSave(
    const char *json, size_t length, ConfigurationSaveRequest &request) {
  request = ConfigurationSaveRequest();
  if (json == nullptr) return ConfigurationMutationParseResult::MALFORMED;
  Parser parser(json, length);
  if (!parser.take('{')) return ConfigurationMutationParseResult::MALFORMED;
  enum : uint8_t { SAVE_REVISION = 1U, SAVE_CONFIRM = 2U };
  uint8_t seen = 0;
  bool first = true;
  while (true) {
    parser.spaces();
    if (parser.take('}')) break;
    if (!first && !parser.take(',')) {
      return ConfigurationMutationParseResult::MALFORMED;
    }
    first = false;
    char key[24];
    if (!parser.string(key, sizeof(key)) || !parser.take(':')) {
      return ConfigurationMutationParseResult::MALFORMED;
    }
    uint8_t field = 0;
    if (strcmp(key, "expected_revision") == 0) field = SAVE_REVISION;
    else if (strcmp(key, "confirm") == 0) field = SAVE_CONFIRM;
    else return ConfigurationMutationParseResult::UNKNOWN_FIELD;
    if ((seen & field) != 0) {
      return ConfigurationMutationParseResult::DUPLICATE_FIELD;
    }
    seen |= field;
    if (field == SAVE_REVISION) {
      int64_t number = 0;
      if (!parser.integer(number)) {
        return ConfigurationMutationParseResult::WRONG_TYPE;
      }
      if (number < 0 || number > UINT32_MAX) {
        return ConfigurationMutationParseResult::OUT_OF_RANGE;
      }
      request.expectedRevision = static_cast<uint32_t>(number);
    } else {
      char confirmation[16];
      if (!parser.string(confirmation, sizeof(confirmation))) {
        return ConfigurationMutationParseResult::WRONG_TYPE;
      }
      if (strcmp(confirmation, "save_wspr") != 0) {
        return ConfigurationMutationParseResult::OUT_OF_RANGE;
      }
    }
  }
  if (!parser.finished()) return ConfigurationMutationParseResult::MALFORMED;
  return seen == (SAVE_REVISION | SAVE_CONFIRM)
             ? ConfigurationMutationParseResult::OK
             : ConfigurationMutationParseResult::MISSING_FIELD;
}
