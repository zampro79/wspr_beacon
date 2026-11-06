#include "ApplicationStatusJson.h"

#include <stdio.h>
#include <string.h>

#include "FirmwareIdentity.h"

#if defined(ARDUINO_ARCH_ESP8266)
  #include <pgmspace.h>
#endif

namespace {
#if defined(ARDUINO_ARCH_ESP8266)
using StoredText = PGM_P;
  #define JSON_STORED_TEXT(text) PSTR(text)
  #define JSON_FLASH_SWITCH \
    __attribute__((optimize("no-jump-tables", "no-tree-switch-conversion")))
#else
using StoredText = const char *;
  #define JSON_STORED_TEXT(text) text
  #define JSON_FLASH_SWITCH
#endif

class JsonWriter {
 public:
  JsonWriter(char *destination, size_t capacity)
      : destination_(destination), capacity_(capacity) {
    if (capacity_ != 0) destination_[0] = '\0';
  }

  bool append(const char *text) {
    if (text == nullptr) return false;
    return appendRaw(text, strlen(text));
  }

  bool appendStored(StoredText text) {
#if defined(ARDUINO_ARCH_ESP8266)
    if (text == nullptr) return false;
    const size_t length = strlen_P(text);
    if (!reserve(length)) return false;
    memcpy_P(destination_ + length_, text, length);
    finishAppend(length);
    return true;
#else
    return append(text);
#endif
  }

  bool string(const char *value) {
    if (!appendStored(JSON_STORED_TEXT("\""))) return false;
    const unsigned char *cursor =
        reinterpret_cast<const unsigned char *>(value == nullptr ? "" : value);
    while (*cursor != 0) {
      const unsigned char c = *cursor++;
      switch (c) {
        case '"': if (!appendStored(JSON_STORED_TEXT("\\\""))) return false; break;
        case '\\': if (!appendStored(JSON_STORED_TEXT("\\\\"))) return false; break;
        case '\b': if (!appendStored(JSON_STORED_TEXT("\\b"))) return false; break;
        case '\f': if (!appendStored(JSON_STORED_TEXT("\\f"))) return false; break;
        case '\n': if (!appendStored(JSON_STORED_TEXT("\\n"))) return false; break;
        case '\r': if (!appendStored(JSON_STORED_TEXT("\\r"))) return false; break;
        case '\t': if (!appendStored(JSON_STORED_TEXT("\\t"))) return false; break;
        default:
          if (c < 0x20U) {
            char escaped[7];
#if defined(ARDUINO_ARCH_ESP8266)
            snprintf_P(escaped, sizeof(escaped), JSON_STORED_TEXT("\\u%04x"),
                       c);
#else
            snprintf(escaped, sizeof(escaped), "\\u%04x", c);
#endif
            if (!append(escaped)) return false;
          } else if (!appendRaw(reinterpret_cast<const char *>(&c), 1)) {
            return false;
          }
      }
    }
    return appendStored(JSON_STORED_TEXT("\""));
  }

  bool storedString(StoredText value) {
    if (!appendStored(JSON_STORED_TEXT("\""))) return false;
#if defined(ARDUINO_ARCH_ESP8266)
    if (value == nullptr) value = JSON_STORED_TEXT("");
    for (size_t index = 0;; ++index) {
      const char c = static_cast<char>(pgm_read_byte(value + index));
      if (c == '\0') break;
      if (!appendRaw(&c, 1U)) return false;
    }
#else
    if (!append(value == nullptr ? "" : value)) return false;
#endif
    return appendStored(JSON_STORED_TEXT("\""));
  }

  bool unsignedNumber(uint32_t value) {
    char number[11];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(number, sizeof(number), JSON_STORED_TEXT("%lu"),
               static_cast<unsigned long>(value));
#else
    snprintf(number, sizeof(number), "%lu", static_cast<unsigned long>(value));
#endif
    return append(number);
  }

  bool signedNumber(int32_t value) {
    char number[12];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(number, sizeof(number), JSON_STORED_TEXT("%ld"),
               static_cast<long>(value));
#else
    snprintf(number, sizeof(number), "%ld", static_cast<long>(value));
#endif
    return append(number);
  }

  bool coordinate(int32_t microdegrees) {
    const bool negative = microdegrees < 0;
    const uint32_t magnitude = negative
        ? static_cast<uint32_t>(-(static_cast<int64_t>(microdegrees)))
        : static_cast<uint32_t>(microdegrees);
    char number[16];
#if defined(ARDUINO_ARCH_ESP8266)
    snprintf_P(number, sizeof(number), JSON_STORED_TEXT("%s%lu.%06lu"),
               negative ? "-" : "",
               static_cast<unsigned long>(magnitude / 1000000UL),
               static_cast<unsigned long>(magnitude % 1000000UL));
#else
    snprintf(number, sizeof(number), "%s%lu.%06lu", negative ? "-" : "",
             static_cast<unsigned long>(magnitude / 1000000UL),
             static_cast<unsigned long>(magnitude % 1000000UL));
#endif
    return append(number);
  }

  bool boolean(bool value) {
    return value ? appendStored(JSON_STORED_TEXT("true"))
                 : appendStored(JSON_STORED_TEXT("false"));
  }
  bool nullValue() { return appendStored(JSON_STORED_TEXT("null")); }
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
    finishAppend(length);
    return true;
  }

  bool reserve(size_t length) {
    if (ok_ && destination_ != nullptr && capacity_ != 0 &&
        length <= capacity_ - 1U - length_) return true;
    ok_ = false;
    if (destination_ != nullptr && capacity_ != 0) destination_[0] = '\0';
    return false;
  }

  void finishAppend(size_t length) {
    length_ += length;
    destination_[length_] = '\0';
  }
};

JSON_FLASH_SWITCH StoredText locatorSourceLabel(LocatorSource source) {
  switch (source) {
    case LocatorSource::FIXED: return JSON_STORED_TEXT("FIXED");
    case LocatorSource::GPS_LIVE: return JSON_STORED_TEXT("GPS_LIVE");
    case LocatorSource::GPS_LAST_KNOWN:
      return JSON_STORED_TEXT("GPS_LAST_KNOWN");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText frequencyModeLabel(WsprFrequencyMode mode) {
  return mode == WsprFrequencyMode::RANDOM ? JSON_STORED_TEXT("RANDOM")
                                           : JSON_STORED_TEXT("FIXED");
}

JSON_FLASH_SWITCH StoredText operatingModeLabel(FirmwareOperatingMode mode) {
  return mode == FirmwareOperatingMode::CONFIGURATION
      ? JSON_STORED_TEXT("CONFIGURATION") : JSON_STORED_TEXT("NORMAL");
}

JSON_FLASH_SWITCH StoredText txStateLabel(ApplicationTxState state) {
  switch (state) {
    case ApplicationTxState::RF_INHIBITED:
      return JSON_STORED_TEXT("RF_INHIBITED");
    case ApplicationTxState::WAITING_FOR_TIME:
      return JSON_STORED_TEXT("WAITING_FOR_TIME");
    case ApplicationTxState::IDLE: return JSON_STORED_TEXT("IDLE");
    case ApplicationTxState::PREPARING:
      return JSON_STORED_TEXT("PREPARING");
    case ApplicationTxState::TRANSMITTING:
      return JSON_STORED_TEXT("TRANSMITTING");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText timeSourceLabel(TimeSource source) {
  switch (source) {
    case TimeSource::NONE: return JSON_STORED_TEXT("NONE");
    case TimeSource::GPS: return JSON_STORED_TEXT("GPS");
    case TimeSource::NTP: return JSON_STORED_TEXT("NTP");
  }
  return JSON_STORED_TEXT("NONE");
}

JSON_FLASH_SWITCH StoredText timeQualityLabel(TimeQuality quality) {
  switch (quality) {
    case TimeQuality::UNKNOWN: return JSON_STORED_TEXT("UNKNOWN");
    case TimeQuality::COARSE: return JSON_STORED_TEXT("COARSE");
    case TimeQuality::SYNCHRONIZED:
      return JSON_STORED_TEXT("SYNCHRONIZED");
    case TimeQuality::PRECISE: return JSON_STORED_TEXT("PRECISE");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText authorizationLabel(
    WsprTimeAuthorization authorization) {
  switch (authorization) {
    case WsprTimeAuthorization::NONE: return JSON_STORED_TEXT("NONE");
    case WsprTimeAuthorization::GPS_PRECISE:
      return JSON_STORED_TEXT("GPS_PRECISE");
    case WsprTimeAuthorization::NTP_EXPERIMENTAL:
      return JSON_STORED_TEXT("NTP_EXPERIMENTAL");
  }
  return JSON_STORED_TEXT("NONE");
}

JSON_FLASH_SWITCH StoredText wifiStateLabel(WifiConnectionState state) {
  switch (state) {
    case WifiConnectionState::STATE_DISABLED:
      return JSON_STORED_TEXT("DISABLED");
    case WifiConnectionState::STATE_DISCONNECTED:
      return JSON_STORED_TEXT("DISCONNECTED");
    case WifiConnectionState::STATE_CONNECTING:
      return JSON_STORED_TEXT("CONNECTING");
    case WifiConnectionState::STATE_CONNECTED:
      return JSON_STORED_TEXT("CONNECTED");
    case WifiConnectionState::STATE_FAILED: return JSON_STORED_TEXT("FAILED");
    case WifiConnectionState::STATE_AP_OPERATIONAL:
      return JSON_STORED_TEXT("AP_OPERATIONAL");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText inhibitReasonLabel(TxInhibitReason reason) {
  switch (reason) {
    case TxInhibitReason::NONE: return JSON_STORED_TEXT("NONE");
    case TxInhibitReason::RF_UNAVAILABLE:
      return JSON_STORED_TEXT("RF_UNAVAILABLE");
    case TxInhibitReason::TIME_UNAVAILABLE:
      return JSON_STORED_TEXT("TIME_UNAVAILABLE");
    case TxInhibitReason::TIME_UNQUALIFIED:
      return JSON_STORED_TEXT("TIME_UNQUALIFIED");
    case TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED:
      return JSON_STORED_TEXT("OPERATING_IDENTITY_UNCONFIGURED");
    case TxInhibitReason::MANUAL_TX_OFF:
      return JSON_STORED_TEXT("MANUAL_TX_OFF");
    case TxInhibitReason::SLOT_MISSED:
      return JSON_STORED_TEXT("SLOT_MISSED");
    case TxInhibitReason::RF_RUNTIME_ERROR:
      return JSON_STORED_TEXT("RF_RUNTIME_ERROR");
  }
  return JSON_STORED_TEXT("NONE");
}

JSON_FLASH_SWITCH StoredText txTimeSourceLabel(TxTimeSource source) {
  switch (source) {
    case TxTimeSource::NONE: return JSON_STORED_TEXT("NONE");
    case TxTimeSource::GPS: return JSON_STORED_TEXT("GPS");
    case TxTimeSource::NTP: return JSON_STORED_TEXT("NTP");
  }
  return JSON_STORED_TEXT("NONE");
}

JSON_FLASH_SWITCH StoredText frameResultLabel(TxFrameResult result) {
  switch (result) {
    case TxFrameResult::NONE: return JSON_STORED_TEXT("NONE");
    case TxFrameResult::COMPLETED_BY_FIRMWARE:
      return JSON_STORED_TEXT("COMPLETED_BY_FIRMWARE");
    case TxFrameResult::INTERRUPTED_RF_ERROR:
      return JSON_STORED_TEXT("INTERRUPTED_RF_ERROR");
    case TxFrameResult::INTERRUPTED_MANUAL_TX_OFF:
      return JSON_STORED_TEXT("INTERRUPTED_MANUAL_TX_OFF");
  }
  return JSON_STORED_TEXT("NONE");
}

JSON_FLASH_SWITCH StoredText diagnosticStateLabel(DiagnosticState state) {
  switch (state) {
    case DiagnosticState::STATE_UNKNOWN: return JSON_STORED_TEXT("UNKNOWN");
    case DiagnosticState::STATE_DISABLED: return JSON_STORED_TEXT("DISABLED");
    case DiagnosticState::STATE_NOT_DETECTED:
      return JSON_STORED_TEXT("NOT_DETECTED");
    case DiagnosticState::STATE_INITIALIZING:
      return JSON_STORED_TEXT("INITIALIZING");
    case DiagnosticState::STATE_OPERATIONAL:
      return JSON_STORED_TEXT("OPERATIONAL");
    case DiagnosticState::STATE_DEGRADED: return JSON_STORED_TEXT("DEGRADED");
    case DiagnosticState::STATE_ERROR: return JSON_STORED_TEXT("ERROR");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText diagnosticReasonLabel(DiagnosticReason reason) {
  switch (reason) {
    case DiagnosticReason::NONE: return JSON_STORED_TEXT("NONE");
    case DiagnosticReason::NOT_EVALUATED:
      return JSON_STORED_TEXT("NOT_EVALUATED");
    case DiagnosticReason::DISABLED_BY_CONFIGURATION:
      return JSON_STORED_TEXT("DISABLED_BY_CONFIGURATION");
    case DiagnosticReason::DEVICE_NOT_FOUND:
      return JSON_STORED_TEXT("DEVICE_NOT_FOUND");
    case DiagnosticReason::INITIALIZATION_FAILED:
      return JSON_STORED_TEXT("INITIALIZATION_FAILED");
    case DiagnosticReason::COMMUNICATION_FAILURE:
      return JSON_STORED_TEXT("COMMUNICATION_FAILURE");
    case DiagnosticReason::TIMEOUT: return JSON_STORED_TEXT("TIMEOUT");
    case DiagnosticReason::INVALID_DATA:
      return JSON_STORED_TEXT("INVALID_DATA");
    case DiagnosticReason::SERVICE_UNAVAILABLE:
      return JSON_STORED_TEXT("SERVICE_UNAVAILABLE");
    case DiagnosticReason::WAITING_FOR_DATA:
      return JSON_STORED_TEXT("WAITING_FOR_DATA");
    case DiagnosticReason::NOT_IMPLEMENTED:
      return JSON_STORED_TEXT("NOT_IMPLEMENTED");
    case DiagnosticReason::RF_OUTPUT_UNVERIFIED:
      return JSON_STORED_TEXT("RF_OUTPUT_UNVERIFIED");
    case DiagnosticReason::INTERNAL_ERROR:
      return JSON_STORED_TEXT("INTERNAL_ERROR");
  }
  return JSON_STORED_TEXT("UNKNOWN");
}

JSON_FLASH_SWITCH StoredText moduleName(size_t index) {
  switch (index) {
    case 0: return JSON_STORED_TEXT("gps");
    case 1: return JSON_STORED_TEXT("pps");
    case 2: return JSON_STORED_TEXT("si5351a");
    case 3: return JSON_STORED_TEXT("oled");
    case 4: return JSON_STORED_TEXT("wifi");
    case 5: return JSON_STORED_TEXT("ntp");
  }
  return JSON_STORED_TEXT("unknown");
}

#if defined(ARDUINO_ARCH_ESP8266)
  #define append(text) appendStored(JSON_STORED_TEXT(text))
#endif

bool appendNullableUnsigned(JsonWriter &writer, bool valid, uint32_t value) {
  return valid ? writer.unsignedNumber(value) : writer.nullValue();
}

bool appendDiagnostic(JsonWriter &writer, const DiagnosticRecord &record,
                      uint32_t nowMillis) {
  const uint32_t age = nowMillis - record.lastChangeMillis;
  const bool ageValid = age <= 0x7FFFFFFFUL;
  return writer.append("{\"state\":") && writer.storedString(diagnosticStateLabel(record.state)) &&
         writer.append(",\"reason\":") && writer.storedString(diagnosticReasonLabel(record.reason)) &&
         writer.append(",\"age_ms\":") && appendNullableUnsigned(writer, ageValid, age) &&
         writer.append(",\"state_changes\":") && writer.unsignedNumber(record.counters.stateChanges) &&
         writer.append(",\"successes\":") && writer.unsignedNumber(record.counters.successes) &&
         writer.append(",\"failures\":") && writer.unsignedNumber(record.counters.failures) &&
         writer.append("}");
}
}  // namespace

bool ApplicationStatusJson::serialize(const ApplicationStatusSnapshot &s,
                                      char *destination, size_t capacity,
                                      size_t &length) {
  length = 0;
  JsonWriter w(destination, capacity);
  const TxFrameSnapshot &last = s.telemetry.lastFrame;
  bool ok =
      w.append("{\"schema\":1,\"firmware\":{\"project\":") &&
      w.string(FirmwareIdentity::project()) &&
      w.append(",\"version\":") && w.string(FirmwareIdentity::version()) &&
      w.append(",\"profile\":") && w.string(FirmwareIdentity::profile()) &&
      w.append(",\"target\":") && w.string(FirmwareIdentity::target()) &&
      w.append(",\"commit\":") && w.string(FirmwareIdentity::commit()) &&
      w.append("},\"identity\":{\"callsign\":") && w.string(s.callsign) &&
      w.append(",\"locator\":") && w.string(s.locator) &&
      w.append(",\"locator_origin\":") && w.storedString(locatorSourceLabel(s.locatorSource)) &&
      w.append(",\"band\":") && w.string(s.band) &&
      w.append(",\"frequency_mode\":") && w.storedString(frequencyModeLabel(s.frequencyMode)) &&
      w.append(",\"power_dbm\":") && w.unsignedNumber(s.powerDbm) &&
      w.append("},\"tx\":{\"state\":") && w.storedString(txStateLabel(s.txState)) &&
      w.append(",\"rf_ready\":") && w.boolean(s.rfReady) &&
      w.append(",\"manual_permission\":") && w.boolean(s.manualTxPermitted) &&
      w.append(",\"effective_eligible\":") && w.boolean(s.effectiveTxEligible) &&
      w.append(",\"inhibit_reason\":") && w.storedString(inhibitReasonLabel(s.telemetry.inhibitReason)) &&
      w.append(",\"configured_frequency_hz\":") && w.unsignedNumber(s.configuredFrequencyHz) &&
      w.append(",\"selected_frequency_hz\":") && appendNullableUnsigned(w, s.selectedFrequencyValid, s.selectedFrequencyHz) &&
      w.append(",\"next_slot_utc_ms_of_day\":") && appendNullableUnsigned(w, s.nextSlotValid, s.nextSlotUtcMillis) &&
      w.append(",\"remaining_ms\":") && appendNullableUnsigned(w, s.nextSlotValid, s.remainingMillis) &&
      w.append(",\"last\":{\"valid\":") && w.boolean(last.valid) &&
      w.append(",\"callsign\":") && (last.valid ? w.string(last.callsign) : w.nullValue()) &&
      w.append(",\"slot_utc_ms_of_day\":") && appendNullableUnsigned(w, last.valid, last.slotUtcMillis) &&
      w.append(",\"frequency_hz\":") && appendNullableUnsigned(w, last.valid, last.frequencyHz) &&
      w.append(",\"locator\":") && (last.valid ? w.string(last.locator) : w.nullValue()) &&
      w.append(",\"power_dbm\":") && appendNullableUnsigned(w, last.valid, last.powerDbm) &&
      w.append(",\"time_source\":") && (last.valid ? w.storedString(txTimeSourceLabel(last.timeSource)) : w.nullValue()) &&
      w.append(",\"result\":") && (last.valid ? w.storedString(frameResultLabel(last.result)) : w.nullValue()) &&
      w.append(",\"end_reason\":") && (last.valid ? w.storedString(inhibitReasonLabel(last.endReason)) : w.nullValue()) &&
      w.append("},\"counters\":{\"started\":") && w.unsignedNumber(s.telemetry.framesStarted) &&
      w.append(",\"completed\":") && w.unsignedNumber(s.telemetry.framesCompleted) &&
      w.append(",\"interrupted\":") && w.unsignedNumber(s.telemetry.framesInterrupted) &&
      w.append("}},\"time\":{\"utc_ms_of_day\":") && appendNullableUnsigned(w, s.time.utcValid(), s.time.utcMillisOfDay) &&
      w.append(",\"source\":") && w.storedString(timeSourceLabel(s.time.source)) &&
      w.append(",\"quality\":") && w.storedString(timeQualityLabel(s.time.quality)) &&
      w.append(",\"tx_authority\":") && w.storedString(authorizationLabel(s.authorization)) &&
      w.append(",\"reference_age_ms\":") && appendNullableUnsigned(w, s.time.utcValid(), s.time.referenceAgeMillis) &&
      w.append("},\"network\":{\"wifi_state\":") && w.storedString(wifiStateLabel(s.wifiState)) &&
      w.append(",\"ipv4\":") && (s.ipv4[0] != '\0' ? w.string(s.ipv4) : w.nullValue()) &&
      w.append("},\"gps\":{\"nmea_available\":") && w.boolean(s.gps.nmeaAvailable) &&
      w.append(",\"fix_valid\":") && w.boolean(s.gps.fixValid) &&
      w.append(",\"satellites\":") && w.unsignedNumber(s.gps.satellites) &&
      w.append(",\"latitude\":") && (s.gps.fixValid ? w.coordinate(s.gps.latitudeMicrodegrees) : w.nullValue()) &&
      w.append(",\"longitude\":") && (s.gps.fixValid ? w.coordinate(s.gps.longitudeMicrodegrees) : w.nullValue()) &&
      w.append(",\"pps_present\":") && w.boolean(s.gps.ppsPresent) &&
      w.append(",\"pps_associated\":") && w.boolean(s.gps.ppsAssociated) &&
      w.append("},\"modules\":{");

  for (size_t i = 0; ok && i < DIAGNOSTIC_MODULE_COUNT; ++i) {
    if (i != 0) ok = w.append(",");
    ok = ok && w.storedString(moduleName(i)) && w.append(":") &&
         appendDiagnostic(w, s.diagnostics.modules[i], s.uptimeMillis);
  }
  const ConfigurationSessionSnapshot &mode = s.operatingMode;
  ok = ok && w.append("},\"operating\":{\"mode\":") &&
       w.storedString(operatingModeLabel(mode.mode)) &&
       w.append(",\"cli_active\":") && w.boolean(mode.cliActive) &&
       w.append(",\"web_active\":") && w.boolean(mode.webActive) &&
       w.append(",\"revision\":") && w.unsignedNumber(mode.revision) &&
       w.append(",\"resume_guard\":") && w.boolean(mode.resumeGuardActive) &&
       w.append(",\"tx_admission_allowed\":") &&
       w.boolean(mode.txAdmissionAllowed) &&
       w.append("},\"system\":{\"board\":") && w.string(s.board) &&
       w.append(",\"uptime_ms\":") && w.unsignedNumber(s.uptimeMillis) &&
       w.append(",\"free_heap_bytes\":") && appendNullableUnsigned(w, s.freeHeapValid, s.freeHeapBytes) &&
       w.append("}}\n");
  if (!ok || !w.ok()) return false;
  length = w.length();
  return length <= MAX_RESPONSE_BYTES;
}

#if defined(ARDUINO_ARCH_ESP8266)
  #undef append
#endif
#undef JSON_STORED_TEXT
#undef JSON_FLASH_SWITCH
