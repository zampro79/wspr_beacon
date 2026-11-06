#include <unity.h>

#include <string.h>

#include <string>

#include "ApplicationStatusJson.h"

namespace {
ApplicationStatusSnapshot populated() {
  ApplicationStatusSnapshot s;
  s.uptimeMillis = 50;
  s.configuredFrequencyHz = 14097100UL;
  s.selectedFrequencyHz = 14097089UL;
  s.nextSlotUtcMillis = 45360000UL;
  s.remainingMillis = 42000UL;
  s.freeHeapBytes = 27104UL;
  strcpy(s.callsign, "IW2NNV");
  strcpy(s.locator, "JN45");
  strcpy(s.band, "20m");
  strcpy(s.ipv4, "192.168.1.254");
  strcpy(s.board, "ESP8266");
  s.powerDbm = 23;
  s.frequencyMode = WsprFrequencyMode::RANDOM;
  s.locatorSource = LocatorSource::GPS_LAST_KNOWN;
  s.txState = ApplicationTxState::IDLE;
  s.authorization = WsprTimeAuthorization::NTP_EXPERIMENTAL;
  s.wifiState = WifiConnectionState::STATE_CONNECTED;
  s.time.utcMillisOfDay = 45296000UL;
  s.time.referenceAgeMillis = 3210UL;
  s.time.source = TimeSource::NTP;
  s.time.quality = TimeQuality::SYNCHRONIZED;
  s.time.statusFlags = 1U;
  s.telemetry.inhibitReason = TxInhibitReason::NONE;
  s.telemetry.framesStarted = 3;
  s.telemetry.framesCompleted = 2;
  s.telemetry.framesInterrupted = 1;
  s.telemetry.lastFrame.valid = true;
  s.telemetry.lastFrame.slotUtcMillis = 45120000UL;
  s.telemetry.lastFrame.frequencyHz = 14097123UL;
  strcpy(s.telemetry.lastFrame.callsign, "IW2NNV");
  strcpy(s.telemetry.lastFrame.locator, "JN45");
  s.telemetry.lastFrame.powerDbm = 10;
  s.telemetry.lastFrame.timeSource = TxTimeSource::NTP;
  s.telemetry.lastFrame.result = TxFrameResult::COMPLETED_BY_FIRMWARE;
  s.telemetry.lastFrame.endReason = TxInhibitReason::NONE;
  s.gps.nmeaAvailable = true;
  s.gps.fixValid = true;
  s.gps.satellites = 8;
  s.gps.latitudeMicrodegrees = 45464000;
  s.gps.longitudeMicrodegrees = -9190000;
  s.gps.ppsPresent = true;
  s.gps.ppsAssociated = false;
  for (size_t i = 0; i < DIAGNOSTIC_MODULE_COUNT; ++i) {
    DiagnosticRecord &record = s.diagnostics.modules[i];
    record.lastChangeMillis = UINT32_MAX - 49UL;
    record.state = i == 2 ? DiagnosticState::STATE_DEGRADED
                          : DiagnosticState::STATE_OPERATIONAL;
    record.reason = i == 2 ? DiagnosticReason::RF_OUTPUT_UNVERIFIED
                           : DiagnosticReason::NONE;
    record.counters.stateChanges = static_cast<uint16_t>(i + 1U);
    record.counters.successes = static_cast<uint16_t>(i + 2U);
    record.counters.failures = static_cast<uint16_t>(i);
  }
  s.rfReady = true;
  s.selectedFrequencyValid = true;
  s.nextSlotValid = true;
  s.freeHeapValid = true;
  s.operatingMode.mode = FirmwareOperatingMode::CONFIGURATION;
  s.operatingMode.revision = 7;
  s.operatingMode.cliActive = true;
  s.operatingMode.webActive = true;
  s.operatingMode.txAdmissionAllowed = false;
  return s;
}

std::string serialize(const ApplicationStatusSnapshot &snapshot) {
  char output[ApplicationStatusJson::MAX_RESPONSE_BYTES + 1U];
  size_t length = 0;
  TEST_ASSERT_TRUE(ApplicationStatusJson::serialize(
      snapshot, output, sizeof(output), length));
  TEST_ASSERT_EQUAL_UINT32(strlen(output), length);
  return std::string(output, length);
}

bool contains(const std::string &json, const char *value) {
  return json.find(value) != std::string::npos;
}

uint32_t fnv1a(const std::string &text) {
  uint32_t hash = 2166136261UL;
  for (const unsigned char value : text) {
    hash ^= value;
    hash *= 16777619UL;
  }
  return hash;
}

std::string stableFirmwareCommit(std::string json) {
  const std::string prefix = "\"commit\":\"";
  const size_t begin = json.find(prefix);
  if (begin == std::string::npos) return json;
  const size_t value = begin + prefix.size();
  const size_t end = json.find('"', value);
  if (end != std::string::npos) json.replace(value, end - value, "<source>");
  return json;
}

class JsonSyntaxValidator {
 public:
  explicit JsonSyntaxValidator(const std::string &text) : text_(text) {}
  bool valid() {
    skipWhitespace();
    if (!value()) return false;
    skipWhitespace();
    return cursor_ == text_.size();
  }

 private:
  const std::string &text_;
  size_t cursor_ = 0;

  void skipWhitespace() {
    while (cursor_ < text_.size() &&
           (text_[cursor_] == ' ' || text_[cursor_] == '\n' ||
            text_[cursor_] == '\r' || text_[cursor_] == '\t')) ++cursor_;
  }
  bool take(char expected) {
    if (cursor_ >= text_.size() || text_[cursor_] != expected) return false;
    ++cursor_;
    return true;
  }
  bool literal(const char *expected) {
    const size_t length = strlen(expected);
    if (text_.compare(cursor_, length, expected) != 0) return false;
    cursor_ += length;
    return true;
  }
  bool string() {
    if (!take('"')) return false;
    while (cursor_ < text_.size()) {
      const unsigned char c = static_cast<unsigned char>(text_[cursor_++]);
      if (c == '"') return true;
      if (c < 0x20U) return false;
      if (c != '\\') continue;
      if (cursor_ >= text_.size()) return false;
      const char escape = text_[cursor_++];
      if (strchr("\"\\/bfnrt", escape) != nullptr) continue;
      if (escape != 'u' || cursor_ + 4U > text_.size()) return false;
      for (uint8_t i = 0; i < 4; ++i) {
        const char hex = text_[cursor_++];
        if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
              (hex >= 'A' && hex <= 'F'))) return false;
      }
    }
    return false;
  }
  bool number() {
    if (cursor_ < text_.size() && text_[cursor_] == '-') ++cursor_;
    const size_t integerStart = cursor_;
    while (cursor_ < text_.size() && text_[cursor_] >= '0' &&
           text_[cursor_] <= '9') ++cursor_;
    if (cursor_ == integerStart) return false;
    if (cursor_ < text_.size() && text_[cursor_] == '.') {
      ++cursor_;
      const size_t fractionStart = cursor_;
      while (cursor_ < text_.size() && text_[cursor_] >= '0' &&
             text_[cursor_] <= '9') ++cursor_;
      if (cursor_ == fractionStart) return false;
    }
    return true;
  }
  bool object() {
    if (!take('{')) return false;
    skipWhitespace();
    if (take('}')) return true;
    while (true) {
      if (!string()) return false;
      skipWhitespace();
      if (!take(':')) return false;
      skipWhitespace();
      if (!value()) return false;
      skipWhitespace();
      if (take('}')) return true;
      if (!take(',')) return false;
      skipWhitespace();
    }
  }
  bool value() {
    skipWhitespace();
    if (cursor_ >= text_.size()) return false;
    if (text_[cursor_] == '{') return object();
    if (text_[cursor_] == '"') return string();
    if (text_[cursor_] == '-' ||
        (text_[cursor_] >= '0' && text_[cursor_] <= '9')) return number();
    return literal("true") || literal("false") || literal("null");
  }
};
}  // namespace

void test_complete_json_has_schema_sections_types_and_units() {
  const std::string json = serialize(populated());
  TEST_ASSERT_TRUE(JsonSyntaxValidator(json).valid());
  TEST_ASSERT_EQUAL_CHAR('{', json.front());
  TEST_ASSERT_EQUAL_CHAR('\n', json.back());
  TEST_ASSERT_TRUE(contains(json, "\"schema\":1"));
  for (const char *section : {"firmware", "identity", "tx", "time",
                              "network", "gps", "modules", "system"}) {
    const std::string token = std::string("\"") + section + "\":{";
    TEST_ASSERT_TRUE(contains(json, token.c_str()));
  }
  TEST_ASSERT_TRUE(contains(json, "\"configured_frequency_hz\":14097100"));
  TEST_ASSERT_TRUE(contains(json, "\"remaining_ms\":42000"));
  TEST_ASSERT_TRUE(contains(json, "\"rf_ready\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"project\":\"WSPR Beacon 2.0\""));
  TEST_ASSERT_TRUE(contains(json, "\"version\":\"dev\""));
  TEST_ASSERT_TRUE(contains(json, "\"target\":\"NATIVE\""));
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_TRUE(contains(json, "\"profile\":\"DEVELOPMENT\""));
#else
  TEST_ASSERT_TRUE(contains(json, "\"profile\":\"PUBLIC\""));
#endif
  TEST_ASSERT_TRUE(json.size() <= ApplicationStatusJson::MAX_RESPONSE_BYTES);
}

void test_fixed_random_prepared_and_last_frame_semantics() {
  ApplicationStatusSnapshot s = populated();
  std::string json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"frequency_mode\":\"RANDOM\""));
  TEST_ASSERT_TRUE(contains(json, "\"selected_frequency_hz\":14097089"));
  TEST_ASSERT_TRUE(contains(json, "\"frequency_hz\":14097123"));
  TEST_ASSERT_TRUE(contains(json, "\"callsign\":\"IW2NNV\""));
  TEST_ASSERT_TRUE(contains(json, "\"locator\":\"JN45\""));
  TEST_ASSERT_TRUE(contains(json, "\"power_dbm\":10"));
  TEST_ASSERT_TRUE(contains(json, "\"result\":\"COMPLETED_BY_FIRMWARE\""));

  s.frequencyMode = WsprFrequencyMode::FIXED;
  s.selectedFrequencyValid = false;
  s.telemetry.lastFrame.valid = false;
  json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"frequency_mode\":\"FIXED\""));
  TEST_ASSERT_TRUE(contains(json, "\"selected_frequency_hz\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"last\":{\"valid\":false"));
  TEST_ASSERT_TRUE(contains(json, "\"callsign\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"frequency_hz\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"locator\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"power_dbm\":null"));
}

void test_unavailable_time_slot_gps_wifi_and_heap_are_null() {
  ApplicationStatusSnapshot s = populated();
  s.time.statusFlags = 0;
  s.time.source = TimeSource::NONE;
  s.time.quality = TimeQuality::UNKNOWN;
  s.authorization = WsprTimeAuthorization::NONE;
  s.nextSlotValid = false;
  s.gps.nmeaAvailable = false;
  s.gps.fixValid = false;
  s.gps.ppsPresent = false;
  s.gps.ppsAssociated = false;
  s.wifiState = WifiConnectionState::STATE_DISCONNECTED;
  s.ipv4[0] = '\0';
  s.freeHeapValid = false;
  const std::string json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"utc_ms_of_day\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"next_slot_utc_ms_of_day\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"remaining_ms\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"latitude\":null,\"longitude\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"wifi_state\":\"DISCONNECTED\",\"ipv4\":null"));
  TEST_ASSERT_TRUE(contains(json, "\"free_heap_bytes\":null"));
}

void test_gps_authority_locator_modules_and_millis_wrap() {
  ApplicationStatusSnapshot s = populated();
  s.authorization = WsprTimeAuthorization::GPS_PRECISE;
  s.time.source = TimeSource::GPS;
  const std::string json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"locator_origin\":\"GPS_LAST_KNOWN\""));
  TEST_ASSERT_TRUE(contains(json, "\"tx_authority\":\"GPS_PRECISE\""));
  TEST_ASSERT_TRUE(contains(json, "\"latitude\":45.464000"));
  TEST_ASSERT_TRUE(contains(json, "\"longitude\":-9.190000"));
  TEST_ASSERT_TRUE(contains(json, "\"si5351a\":{\"state\":\"DEGRADED\""));
  TEST_ASSERT_TRUE(contains(json, "\"age_ms\":100"));
}

void test_strings_are_escaped_and_credentials_are_absent() {
  ApplicationStatusSnapshot s = populated();
  strcpy(s.callsign, "A\"B\\C");
  strcpy(s.telemetry.lastFrame.callsign, "A\"B\\C");
  strcpy(s.board, "ESP\n8266");
  const std::string json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "A\\\"B\\\\C"));
  TEST_ASSERT_TRUE(contains(json, "ESP\\n8266"));
  TEST_ASSERT_FALSE(contains(json, "ssid"));
  TEST_ASSERT_FALSE(contains(json, "password"));
  TEST_ASSERT_FALSE(contains(json, "secret"));
  TEST_ASSERT_FALSE(contains(json, "session_id"));
  TEST_ASSERT_FALSE(contains(json, "csrf"));
}

void test_operating_mode_is_additive_and_authoritative() {
  ApplicationStatusSnapshot s = populated();
  std::string json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"operating\":{\"mode\":\"CONFIGURATION\""));
  TEST_ASSERT_TRUE(contains(json, "\"cli_active\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"web_active\":true"));
  TEST_ASSERT_TRUE(contains(json, "\"revision\":7"));
  TEST_ASSERT_TRUE(contains(json, "\"resume_guard\":false"));
  TEST_ASSERT_TRUE(contains(json, "\"tx_admission_allowed\":false"));

  s.operatingMode = ConfigurationSessionSnapshot();
  s.operatingMode.resumeGuardActive = true;
  s.operatingMode.txAdmissionAllowed = false;
  json = serialize(s);
  TEST_ASSERT_TRUE(contains(json, "\"mode\":\"NORMAL\""));
  TEST_ASSERT_TRUE(contains(json, "\"resume_guard\":true"));
}

void test_all_diagnostic_labels_and_output_bytes_are_stable() {
  ApplicationStatusSnapshot s = populated();
  const DiagnosticState states[] = {
      DiagnosticState::STATE_UNKNOWN,       DiagnosticState::STATE_DISABLED,
      DiagnosticState::STATE_NOT_DETECTED,  DiagnosticState::STATE_INITIALIZING,
      DiagnosticState::STATE_OPERATIONAL,   DiagnosticState::STATE_DEGRADED,
      DiagnosticState::STATE_ERROR};
  const char *stateLabels[] = {"UNKNOWN",      "DISABLED", "NOT_DETECTED",
                               "INITIALIZING", "OPERATIONAL", "DEGRADED",
                               "ERROR"};
  for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
    s.diagnostics.modules[0].state = states[i];
    TEST_ASSERT_TRUE(contains(serialize(s), stateLabels[i]));
  }

  const DiagnosticReason reasons[] = {
      DiagnosticReason::NONE,
      DiagnosticReason::NOT_EVALUATED,
      DiagnosticReason::DISABLED_BY_CONFIGURATION,
      DiagnosticReason::DEVICE_NOT_FOUND,
      DiagnosticReason::INITIALIZATION_FAILED,
      DiagnosticReason::COMMUNICATION_FAILURE,
      DiagnosticReason::TIMEOUT,
      DiagnosticReason::INVALID_DATA,
      DiagnosticReason::SERVICE_UNAVAILABLE,
      DiagnosticReason::WAITING_FOR_DATA,
      DiagnosticReason::NOT_IMPLEMENTED,
      DiagnosticReason::RF_OUTPUT_UNVERIFIED,
      DiagnosticReason::INTERNAL_ERROR};
  const char *reasonLabels[] = {
      "NONE",          "NOT_EVALUATED",       "DISABLED_BY_CONFIGURATION",
      "DEVICE_NOT_FOUND", "INITIALIZATION_FAILED", "COMMUNICATION_FAILURE",
      "TIMEOUT",       "INVALID_DATA",        "SERVICE_UNAVAILABLE",
      "WAITING_FOR_DATA", "NOT_IMPLEMENTED",  "RF_OUTPUT_UNVERIFIED",
      "INTERNAL_ERROR"};
  for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); ++i) {
    s.diagnostics.modules[0].reason = reasons[i];
    TEST_ASSERT_TRUE(contains(serialize(s), reasonLabels[i]));
  }

#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_EQUAL_HEX32(
      0xCDE19877UL, fnv1a(stableFirmwareCommit(serialize(populated()))));
#else
  TEST_ASSERT_EQUAL_HEX32(
      0xE4819CB9UL, fnv1a(stableFirmwareCommit(serialize(populated()))));
#endif
}

void test_bounded_buffer_fails_without_truncated_json() {
  ApplicationStatusSnapshot s = populated();
  char small[128];
  size_t length = 99;
  TEST_ASSERT_FALSE(
      ApplicationStatusJson::serialize(s, small, sizeof(small), length));
  TEST_ASSERT_EQUAL_UINT32(0, length);
  TEST_ASSERT_EQUAL_CHAR('\0', small[0]);

  strcpy(s.board, "ESP8266-MAXIMUM");
  for (size_t i = 0; i < DIAGNOSTIC_MODULE_COUNT; ++i) {
    s.diagnostics.modules[i].reason =
        DiagnosticReason::DISABLED_BY_CONFIGURATION;
    s.diagnostics.modules[i].counters.stateChanges = UINT16_MAX;
    s.diagnostics.modules[i].counters.successes = UINT16_MAX;
    s.diagnostics.modules[i].counters.failures = UINT16_MAX;
  }
  char maximum[ApplicationStatusJson::MAX_RESPONSE_BYTES + 1U];
  TEST_ASSERT_TRUE(ApplicationStatusJson::serialize(
      s, maximum, sizeof(maximum), length));
  TEST_ASSERT_TRUE(length <= ApplicationStatusJson::MAX_RESPONSE_BYTES);
  TEST_ASSERT_TRUE(JsonSyntaxValidator(std::string(maximum, length)).valid());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_complete_json_has_schema_sections_types_and_units);
  RUN_TEST(test_fixed_random_prepared_and_last_frame_semantics);
  RUN_TEST(test_unavailable_time_slot_gps_wifi_and_heap_are_null);
  RUN_TEST(test_gps_authority_locator_modules_and_millis_wrap);
  RUN_TEST(test_strings_are_escaped_and_credentials_are_absent);
  RUN_TEST(test_operating_mode_is_additive_and_authoritative);
  RUN_TEST(test_all_diagnostic_labels_and_output_bytes_are_stable);
  RUN_TEST(test_bounded_buffer_fails_without_truncated_json);
  return UNITY_END();
}
