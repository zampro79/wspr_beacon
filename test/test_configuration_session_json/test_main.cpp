#include <unity.h>

#include <cstring>
#include <string>

#include "ConfigurationSessionJson.h"

namespace {
constexpr char OWNER[] = "00112233445566778899aabbccddeeff";
}

void test_enter_requires_exact_bounded_fields() {
  ConfigurationSessionEnterRequest request;
  const std::string valid =
      std::string("{\"session_id\":\"") + OWNER +
      "\",\"expected_revision\":4294967295}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionParseResult::OK),
      static_cast<uint8_t>(ConfigurationSessionJson::parseEnter(
          valid.c_str(), valid.size(), request)));
  TEST_ASSERT_EQUAL_STRING(OWNER, request.sessionId);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, request.expectedRevision);

  const char uppercase[] =
      "{\"session_id\":\"00112233445566778899AABBCCDDEEFF\","
      "\"expected_revision\":0}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(ConfigurationSessionJson::parseEnter(
          uppercase, sizeof(uppercase) - 1U, request)));
  const char missing[] = "{\"session_id\":\"00112233445566778899aabbccddeeff\"}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationSessionParseResult::MISSING_FIELD),
      static_cast<uint8_t>(ConfigurationSessionJson::parseEnter(
          missing, sizeof(missing) - 1U, request)));
}

void test_owner_body_rejects_revision_unknown_and_duplicate_fields() {
  char owner[33];
  const char valid[] = "{\"session_id\":\"00112233445566778899aabbccddeeff\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionParseResult::OK),
      static_cast<uint8_t>(ConfigurationSessionJson::parseOwner(valid, sizeof(valid)-1U, owner)));
  TEST_ASSERT_EQUAL_STRING(OWNER, owner);
  const char extra[] = "{\"session_id\":\"00112233445566778899aabbccddeeff\",\"expected_revision\":1}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationSessionParseResult::UNKNOWN_FIELD),
      static_cast<uint8_t>(ConfigurationSessionJson::parseOwner(extra, sizeof(extra)-1U, owner)));
}

void test_session_snapshot_contains_no_owner_or_token() {
  ConfigurationSessionSnapshot snapshot;
  snapshot.mode = FirmwareOperatingMode::CONFIGURATION;
  snapshot.revision = 7;
  snapshot.cliActive = true;
  snapshot.webActive = true;
  snapshot.webRemainingMillis = 12345;
  snapshot.txAdmissionAllowed = false;
  char output[ConfigurationSessionJson::MAX_RESPONSE_BYTES + 1U];
  size_t length = 0;
  TEST_ASSERT_TRUE(ConfigurationSessionJson::serialize(
      snapshot, output, sizeof(output), length));
  const std::string json(output, length);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"mode\":\"CONFIGURATION\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"web_expiry_remaining_ms\":12345"));
  TEST_ASSERT_EQUAL(std::string::npos, json.find(OWNER));
  TEST_ASSERT_EQUAL(std::string::npos, json.find("csrf"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_enter_requires_exact_bounded_fields);
  RUN_TEST(test_owner_body_rejects_revision_unknown_and_duplicate_fields);
  RUN_TEST(test_session_snapshot_contains_no_owner_or_token);
  return UNITY_END();
}
