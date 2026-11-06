#include "ConfigurationSaveJson.h"

#include <stdio.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP8266)
  #include <pgmspace.h>
#endif

namespace {
#if defined(ARDUINO_ARCH_ESP8266)
using StoredText = PGM_P;
  #define SAVE_JSON_TEXT(text) PSTR(text)
#else
using StoredText = const char *;
  #define SAVE_JSON_TEXT(text) text
#endif

class Writer {
 public:
  Writer(char *destination, size_t capacity)
      : destination_(destination), capacity_(capacity) {
    if (destination_ != nullptr && capacity_ != 0) destination_[0] = '\0';
  }

  bool stored(StoredText text) {
#if defined(ARDUINO_ARCH_ESP8266)
    if (text == nullptr) return false;
    const size_t count = strlen_P(text);
    if (!reserve(count)) return false;
    memcpy_P(destination_ + length_, text, count);
#else
    if (text == nullptr) return false;
    const size_t count = strlen(text);
    if (!reserve(count)) return false;
    memcpy(destination_ + length_, text, count);
#endif
    length_ += count;
    destination_[length_] = '\0';
    return true;
  }

  bool number(uint32_t value) {
    char buffer[11];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(buffer, sizeof(buffer), SAVE_JSON_TEXT("%lu"),
               static_cast<unsigned long>(value));
#else
    snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(value));
#endif
    const size_t count = strlen(buffer);
    if (!reserve(count)) return false;
    memcpy(destination_ + length_, buffer, count);
    length_ += count;
    destination_[length_] = '\0';
    return true;
  }

  bool boolean(bool value) {
    return stored(value ? SAVE_JSON_TEXT("true") : SAVE_JSON_TEXT("false"));
  }
  bool ok() const { return ok_; }
  size_t length() const { return length_; }

 private:
  char *destination_;
  size_t capacity_;
  size_t length_ = 0;
  bool ok_ = true;

  bool reserve(size_t count) {
    if (ok_ && destination_ != nullptr && capacity_ != 0 &&
        count <= capacity_ - 1U - length_) return true;
    ok_ = false;
    if (destination_ != nullptr && capacity_ != 0) destination_[0] = '\0';
    return false;
  }
};

StoredText resultText(WsprSaveResult result) {
  switch (result) {
    case WsprSaveResult::SAVED_APPLIED: return SAVE_JSON_TEXT("saved_applied");
    case WsprSaveResult::UNCHANGED: return SAVE_JSON_TEXT("unchanged");
    case WsprSaveResult::STALE_REVISION: return SAVE_JSON_TEXT("stale_revision");
    case WsprSaveResult::CONFIG_MODE_REQUIRED: return SAVE_JSON_TEXT("invalid_session");
    case WsprSaveResult::RF_UNSAFE: return SAVE_JSON_TEXT("rf_unsafe");
    case WsprSaveResult::INVALID_CONFIGURATION: return SAVE_JSON_TEXT("invalid_configuration");
    case WsprSaveResult::PERSISTENCE_FAILED: return SAVE_JSON_TEXT("persistence_failed");
    case WsprSaveResult::VERIFICATION_FAILED: return SAVE_JSON_TEXT("verification_failed");
    case WsprSaveResult::PERSISTED_APPLY_FAILED: return SAVE_JSON_TEXT("persisted_apply_failed");
  }
  return SAVE_JSON_TEXT("persistence_failed");
}
}  // namespace

bool ConfigurationSaveJson::serialize(const WsprSaveOutcome &outcome,
                                      uint32_t revision, char *destination,
                                      size_t capacity, size_t &length) {
  Writer writer(destination, capacity);
  const bool success =
      writer.stored(SAVE_JSON_TEXT("{\"schema\":1,\"result\":\"")) &&
      writer.stored(resultText(outcome.result)) &&
      writer.stored(SAVE_JSON_TEXT("\",\"revision\":")) &&
      writer.number(revision) &&
      writer.stored(SAVE_JSON_TEXT(",\"persisted\":")) &&
      writer.boolean(outcome.persistenceOccurred) &&
      writer.stored(SAVE_JSON_TEXT(",\"applied\":")) &&
      writer.boolean(outcome.runtimeApplySucceeded) &&
      writer.stored(SAVE_JSON_TEXT("}\n"));
  length = success && writer.ok() ? writer.length() : 0;
  return length != 0;
}
