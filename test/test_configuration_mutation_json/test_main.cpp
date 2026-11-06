#include <unity.h>
#include <string.h>

#include "ConfigurationMutationJson.h"

namespace {
const char VALID[] =
    "{\"callsign\":\"IW2NNV\",\"locator\":\"JN45\",\"band\":\"20m\","
    "\"frequency_mode\":\"RANDOM\",\"power_save\":true,\"frequency_hz\":14097100,"
    "\"offset_hz\":-2,\"correction\":76000,\"power_dbm\":10,"
    "\"interval_min\":4,\"expected_revision\":7}";
}

void test_complete_candidate_parses_exact_fields_and_types() {
  ConfigurationCandidateRequest request;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::OK),
      static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(
          VALID, sizeof(VALID) - 1U, request)));
  TEST_ASSERT_EQUAL_STRING("IW2NNV", request.settings.callsign);
  TEST_ASSERT_EQUAL_STRING("JN45", request.settings.fixedLocator);
  TEST_ASSERT_EQUAL_INT16(-2, request.settings.frequencyOffsetHz);
  TEST_ASSERT_EQUAL_UINT32(7, request.expectedRevision);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(request.settings.frequencyMode));
  TEST_ASSERT_TRUE(request.settings.powerSave);
}

void test_unknown_duplicate_missing_and_trailing_fields_are_rejected() {
  ConfigurationCandidateRequest request;
  const char unknown[] = "{\"expected_revision\":1,\"extra\":2}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::UNKNOWN_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(unknown, sizeof(unknown)-1, request)));
  const char duplicate[] = "{\"expected_revision\":1,\"expected_revision\":2}";
  uint32_t revision = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::DUPLICATE_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseRevision(duplicate, sizeof(duplicate)-1, revision)));
  const char missing[] = "{\"expected_revision\":1}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::MISSING_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(missing, sizeof(missing)-1, request)));
  const char trailing[] = "{\"expected_revision\":1}x";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::MALFORMED),
      static_cast<uint8_t>(ConfigurationMutationJson::parseRevision(trailing, sizeof(trailing)-1, revision)));
}

void test_wrong_types_overflow_and_unsupported_values_are_rejected() {
  uint32_t revision = 0;
  const char wrong[] = "{\"expected_revision\":\"1\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::WRONG_TYPE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseRevision(wrong, sizeof(wrong)-1, revision)));
  const char overflow[] = "{\"expected_revision\":4294967296}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseRevision(overflow, sizeof(overflow)-1, revision)));
  char invalidBand[sizeof(VALID)];
  strcpy(invalidBand, VALID);
  char *band = strstr(invalidBand, "20m");
  memcpy(band, "80m", 3);
  ConfigurationCandidateRequest request;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(invalidBand, strlen(invalidBand), request)));
  char wrongBoolean[sizeof(VALID) + 4U];
  strcpy(wrongBoolean, VALID);
  char *powerSave = strstr(wrongBoolean, "true");
  memmove(powerSave + 3, powerSave + 4, strlen(powerSave + 4) + 1U);
  memcpy(powerSave, "\"on", 3);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::WRONG_TYPE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(
          wrongBoolean, strlen(wrongBoolean), request)));
}

void test_every_tx_enabled_band_name_is_parsed_exactly() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    char candidate[sizeof(VALID)];
    strcpy(candidate, VALID);
    memcpy(strstr(candidate, "20m"), band->name, 3);
    ConfigurationCandidateRequest request;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConfigurationMutationParseResult::OK),
        static_cast<uint8_t>(ConfigurationMutationJson::parseCandidate(
            candidate, strlen(candidate), request)));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(band->id),
                            static_cast<uint8_t>(request.settings.band));
  }
}

void test_discard_body_accepts_only_expected_revision() {
  uint32_t revision = 0;
  const char body[] = " { \"expected_revision\" : 4294967295 } \n";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::OK),
      static_cast<uint8_t>(ConfigurationMutationJson::parseRevision(body, sizeof(body)-1, revision)));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, revision);
}

void test_save_body_requires_exact_revision_and_confirmation() {
  ConfigurationSaveRequest request;
  const char valid[] =
      "{\"expected_revision\":12,\"confirm\":\"save_wspr\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationMutationParseResult::OK),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          valid, sizeof(valid) - 1U, request)));
  TEST_ASSERT_EQUAL_UINT32(12, request.expectedRevision);

  const char wrong[] =
      "{\"expected_revision\":12,\"confirm\":\"save_all\"}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          wrong, sizeof(wrong) - 1U, request)));
  const char missing[] = "{\"expected_revision\":12}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::MISSING_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          missing, sizeof(missing) - 1U, request)));
}

void test_save_body_rejects_duplicate_unknown_type_overflow_and_trailing_data() {
  ConfigurationSaveRequest request;
  const char duplicate[] =
      "{\"expected_revision\":1,\"confirm\":\"save_wspr\","
      "\"confirm\":\"save_wspr\"}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::DUPLICATE_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          duplicate, sizeof(duplicate) - 1U, request)));
  const char unknown[] =
      "{\"expected_revision\":1,\"confirm\":\"save_wspr\",\"x\":0}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::UNKNOWN_FIELD),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          unknown, sizeof(unknown) - 1U, request)));
  const char wrongType[] =
      "{\"expected_revision\":1,\"confirm\":true}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::WRONG_TYPE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          wrongType, sizeof(wrongType) - 1U, request)));
  const char overflow[] =
      "{\"expected_revision\":4294967296,\"confirm\":\"save_wspr\"}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::OUT_OF_RANGE),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          overflow, sizeof(overflow) - 1U, request)));
  const char trailing[] =
      "{\"expected_revision\":1,\"confirm\":\"save_wspr\"}x";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigurationMutationParseResult::MALFORMED),
      static_cast<uint8_t>(ConfigurationMutationJson::parseSave(
          trailing, sizeof(trailing) - 1U, request)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_complete_candidate_parses_exact_fields_and_types);
  RUN_TEST(test_unknown_duplicate_missing_and_trailing_fields_are_rejected);
  RUN_TEST(test_wrong_types_overflow_and_unsupported_values_are_rejected);
  RUN_TEST(test_every_tx_enabled_band_name_is_parsed_exactly);
  RUN_TEST(test_discard_body_accepts_only_expected_revision);
  RUN_TEST(test_save_body_requires_exact_revision_and_confirmation);
  RUN_TEST(test_save_body_rejects_duplicate_unknown_type_overflow_and_trailing_data);
  return UNITY_END();
}
