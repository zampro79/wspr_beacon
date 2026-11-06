#include "ConfigurationJson.h"

#include <stdio.h>
#include <string.h>

#include "BandPlan.h"
#include "ConfigValidator.h"
#include "PersistentConfigRecord.h"

#if defined(ARDUINO_ARCH_ESP8266)
  #include <pgmspace.h>
#endif

namespace {
#if defined(ARDUINO_ARCH_ESP8266)
using StoredText = PGM_P;
  #define CONFIG_JSON_TEXT(text) PSTR(text)
#else
using StoredText = const char *;
  #define CONFIG_JSON_TEXT(text) text
#endif

class JsonWriter {
 public:
  JsonWriter(char *destination, size_t capacity)
      : destination_(destination), capacity_(capacity) {
    if (destination_ != nullptr && capacity_ != 0) destination_[0] = '\0';
  }

  bool append(const char *text) {
    return text != nullptr && appendRaw(text, strlen(text));
  }

  bool appendStored(StoredText text) {
#if defined(ARDUINO_ARCH_ESP8266)
    if (text == nullptr) return false;
    const size_t length = strlen_P(text);
    if (!reserve(length)) return false;
    memcpy_P(destination_ + length_, text, length);
    finish(length);
    return true;
#else
    return append(text);
#endif
  }

  bool string(const char *value) {
    if (!appendStored(CONFIG_JSON_TEXT("\""))) return false;
    const unsigned char *cursor = reinterpret_cast<const unsigned char *>(
        value == nullptr ? "" : value);
    while (*cursor != 0) {
      const unsigned char c = *cursor++;
      switch (c) {
        case '"': if (!appendStored(CONFIG_JSON_TEXT("\\\""))) return false; break;
        case '\\': if (!appendStored(CONFIG_JSON_TEXT("\\\\"))) return false; break;
        case '\b': if (!appendStored(CONFIG_JSON_TEXT("\\b"))) return false; break;
        case '\f': if (!appendStored(CONFIG_JSON_TEXT("\\f"))) return false; break;
        case '\n': if (!appendStored(CONFIG_JSON_TEXT("\\n"))) return false; break;
        case '\r': if (!appendStored(CONFIG_JSON_TEXT("\\r"))) return false; break;
        case '\t': if (!appendStored(CONFIG_JSON_TEXT("\\t"))) return false; break;
        default:
          if (c < 0x20U) {
            char escaped[7];
#if defined(ARDUINO_ARCH_ESP8266)
            snprintf_P(escaped, sizeof(escaped), CONFIG_JSON_TEXT("\\u%04x"), c);
#else
            snprintf(escaped, sizeof(escaped), "\\u%04x", c);
#endif
            if (!append(escaped)) return false;
          } else if (!appendRaw(reinterpret_cast<const char *>(&c), 1U)) {
            return false;
          }
      }
    }
    return appendStored(CONFIG_JSON_TEXT("\""));
  }

  bool storedString(StoredText value) {
    if (!appendStored(CONFIG_JSON_TEXT("\""))) return false;
#if defined(ARDUINO_ARCH_ESP8266)
    if (value == nullptr) value = CONFIG_JSON_TEXT("");
    for (size_t index = 0;; ++index) {
      const char c = static_cast<char>(pgm_read_byte(value + index));
      if (c == '\0') break;
      if (!appendRaw(&c, 1U)) return false;
    }
#else
    if (!append(value == nullptr ? "" : value)) return false;
#endif
    return appendStored(CONFIG_JSON_TEXT("\""));
  }

  bool unsignedNumber(uint32_t value) {
    char number[11];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(number, sizeof(number), CONFIG_JSON_TEXT("%lu"),
               static_cast<unsigned long>(value));
#else
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(value));
#endif
    return append(number);
  }

  bool signedNumber(int32_t value) {
    char number[12];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(number, sizeof(number), CONFIG_JSON_TEXT("%ld"),
               static_cast<long>(value));
#else
    snprintf(number, sizeof(number), "%ld", static_cast<long>(value));
#endif
    return append(number);
  }

  bool boolean(bool value) {
    return appendStored(value ? CONFIG_JSON_TEXT("true")
                              : CONFIG_JSON_TEXT("false"));
  }
  bool nullValue() { return appendStored(CONFIG_JSON_TEXT("null")); }
  bool ok() const { return ok_; }
  size_t length() const { return length_; }

 private:
  char *destination_;
  size_t capacity_;
  size_t length_ = 0;
  bool ok_ = true;

  bool appendRaw(const char *data, size_t length) {
    if (!reserve(length)) return false;
    memcpy(destination_ + length_, data, length);
    finish(length);
    return true;
  }

  bool reserve(size_t length) {
    if (ok_ && destination_ != nullptr && capacity_ != 0 &&
        length <= capacity_ - 1U - length_) return true;
    ok_ = false;
    if (destination_ != nullptr && capacity_ != 0) destination_[0] = '\0';
    return false;
  }

  void finish(size_t length) {
    length_ += length;
    destination_[length_] = '\0';
  }
};

StoredText modeLabel(WsprFrequencyMode mode) {
  return mode == WsprFrequencyMode::RANDOM ? CONFIG_JSON_TEXT("RANDOM")
                                           : CONFIG_JSON_TEXT("FIXED");
}

StoredText originLabel(ConfigOrigin origin) {
  return origin == ConfigOrigin::SAVED ? CONFIG_JSON_TEXT("SAVED")
                                       : CONFIG_JSON_TEXT("DEFAULTS");
}

bool appendSettings(JsonWriter &writer, const BeaconSettings &settings) {
  const BandPlanEntry *band = BandPlan::find(settings.band);
  return writer.appendStored(CONFIG_JSON_TEXT("{\"callsign\":")) &&
         writer.string(settings.callsign) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"locator\":")) &&
         writer.string(settings.fixedLocator) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"band\":")) &&
         (band != nullptr ? writer.string(band->name) : writer.nullValue()) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"frequency_mode\":")) &&
         writer.storedString(modeLabel(settings.frequencyMode)) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"power_save\":")) &&
         writer.boolean(settings.powerSave) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"frequency_hz\":")) &&
         writer.unsignedNumber(settings.baseFrequencyHz) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"offset_hz\":")) &&
         writer.signedNumber(settings.frequencyOffsetHz) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"correction\":")) &&
         writer.signedNumber(settings.si5351Correction) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"power_dbm\":")) &&
         writer.unsignedNumber(settings.powerDbm) &&
         writer.appendStored(CONFIG_JSON_TEXT(",\"interval_min\":")) &&
         writer.unsignedNumber(settings.transmitIntervalMinutes) &&
         writer.appendStored(CONFIG_JSON_TEXT("}"));
}

bool appendSelectableBands(JsonWriter &writer) {
  bool first = true;
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    if (band == nullptr || !band->txEnabled) continue;
    if (!first && !writer.appendStored(CONFIG_JSON_TEXT(","))) return false;
    first = false;
    if (!writer.appendStored(CONFIG_JSON_TEXT("{\"id\":")) ||
        !writer.unsignedNumber(static_cast<uint8_t>(band->id)) ||
        !writer.appendStored(CONFIG_JSON_TEXT(",\"name\":")) ||
        !writer.string(band->name) ||
        !writer.appendStored(CONFIG_JSON_TEXT(",\"tx_enabled\":true,")) ||
        !writer.appendStored(CONFIG_JSON_TEXT("\"nominal_frequency_hz\":")) ||
        !writer.unsignedNumber(band->nominalToneZeroHz) ||
        !writer.appendStored(CONFIG_JSON_TEXT(",")) ||
        !writer.appendStored(CONFIG_JSON_TEXT("\"frequency_min_hz\":")) ||
        !writer.unsignedNumber(band->lowerLimitHz) ||
        !writer.appendStored(CONFIG_JSON_TEXT(",\"frequency_max_hz\":")) ||
        !writer.unsignedNumber(band->upperLimitHz) ||
        !writer.appendStored(CONFIG_JSON_TEXT("}"))) return false;
  }
  return true;
}

bool appendPowerValues(JsonWriter &writer) {
  for (uint8_t index = 0; index < ConfigValidator::allowedPowerCount(); ++index) {
    uint8_t value = 0;
    if (!ConfigValidator::allowedPowerAt(index, value)) return false;
    if (index != 0 && !writer.appendStored(CONFIG_JSON_TEXT(","))) return false;
    if (!writer.unsignedNumber(value)) return false;
  }
  return true;
}
}  // namespace

bool ConfigurationJson::serialize(const ConfigurationSnapshot &snapshot,
                                  char *destination, size_t capacity,
                                  size_t &length) {
  length = 0;
  JsonWriter writer(destination, capacity);
  bool ok = writer.appendStored(CONFIG_JSON_TEXT("{\"schema\":1,\"revision\":")) &&
            writer.unsignedNumber(snapshot.revision) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"active\":")) &&
            appendSettings(writer, snapshot.active) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"candidate\":")) &&
            appendSettings(writer, snapshot.candidate) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"saved\":")) &&
            appendSettings(writer, snapshot.saved) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"state\":{\"candidate_dirty\":")) &&
            writer.boolean(snapshot.status.wsprDirty) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"active_differs_saved\":")) &&
            writer.boolean(snapshot.activeDiffersSaved) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"wspr_origin\":")) &&
            writer.storedString(originLabel(snapshot.status.wsprOrigin)) &&
            writer.appendStored(CONFIG_JSON_TEXT(",\"persistence_version\":"));
  if (ok) {
    ok = snapshot.status.wsprPayloadVersion == 0
             ? writer.nullValue()
             : writer.unsignedNumber(snapshot.status.wsprPayloadVersion);
  }
  ok = ok && writer.appendStored(CONFIG_JSON_TEXT(",\"migration_pending\":")) &&
       writer.boolean(snapshot.status.wsprMigrationPending) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"runtime_synchronized\":")) &&
       writer.boolean(snapshot.runtimeSynchronized) &&
       writer.appendStored(CONFIG_JSON_TEXT("},\"capabilities\":{\"selectable_bands\":[")) &&
       appendSelectableBands(writer) &&
       writer.appendStored(CONFIG_JSON_TEXT("],\"frequency_modes\":[\"FIXED\",\"RANDOM\"],\"power_dbm\":[")) &&
       appendPowerValues(writer) &&
       writer.appendStored(CONFIG_JSON_TEXT("],\"interval_min\":{\"min\":")) &&
       writer.unsignedNumber(ConfigValidator::MIN_INTERVAL_MINUTES) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"max\":")) &&
       writer.unsignedNumber(ConfigValidator::MAX_INTERVAL_MINUTES) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"step\":")) &&
       writer.unsignedNumber(ConfigValidator::INTERVAL_STEP_MINUTES) &&
       writer.appendStored(CONFIG_JSON_TEXT("},\"offset_hz\":{\"min\":")) &&
       writer.signedNumber(ConfigValidator::MIN_OFFSET_HZ) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"max\":")) &&
       writer.signedNumber(ConfigValidator::MAX_OFFSET_HZ) &&
       writer.appendStored(CONFIG_JSON_TEXT("},\"correction\":{\"min\":")) &&
       writer.signedNumber(ConfigValidator::MIN_CORRECTION) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"max\":")) &&
       writer.signedNumber(ConfigValidator::MAX_CORRECTION) &&
       writer.appendStored(CONFIG_JSON_TEXT("},\"callsign\":{\"min_length\":")) &&
       writer.unsignedNumber(ConfigValidator::MIN_CALLSIGN_LENGTH) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"max_length\":")) &&
       writer.unsignedNumber(ConfigValidator::MAX_CALLSIGN_LENGTH) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"format\":\"WSPR_TYPE_1\"},\"locator\":{\"length\":")) &&
       writer.unsignedNumber(ConfigValidator::LOCATOR_LENGTH) &&
       writer.appendStored(CONFIG_JSON_TEXT(",\"format\":\"AA00-RR99\"}}}\n"));
  if (!ok || !writer.ok()) return false;
  length = writer.length();
  return length <= MAX_RESPONSE_BYTES;
}

#undef CONFIG_JSON_TEXT
