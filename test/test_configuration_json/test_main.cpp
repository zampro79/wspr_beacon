#include <unity.h>

#include <cstring>
#include <string>

#include "ConfigurationJson.h"
#include "ConfigValidator.h"
#include "PersistentConfigRecord.h"

namespace {
ConfigurationSnapshot baseSnapshot() {
  ConfigurationSnapshot snapshot;
  snapshot.revision = 7;
  snapshot.active = BeaconSettings();
  snapshot.candidate = snapshot.active;
  snapshot.saved = snapshot.active;
  snapshot.status.wsprOrigin = ConfigOrigin::SAVED;
  snapshot.status.wsprPayloadVersion =
      PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V3;
  return snapshot;
}

std::string serialize(const ConfigurationSnapshot &snapshot) {
  char output[ConfigurationJson::MAX_RESPONSE_BYTES + 1U];
  size_t length = 0;
  TEST_ASSERT_TRUE(ConfigurationJson::serialize(
      snapshot, output, sizeof(output), length));
  TEST_ASSERT_EQUAL_UINT32(strlen(output), length);
  return std::string(output, length);
}

bool structurallyValidJson(const std::string &json) {
  int braces = 0;
  int brackets = 0;
  bool quoted = false;
  bool escaped = false;
  for (char value : json) {
    if (quoted) {
      if (escaped) {
        escaped = false;
      } else if (value == '\\') {
        escaped = true;
      } else if (value == '"') {
        quoted = false;
      }
      continue;
    }
    if (value == '"') quoted = true;
    else if (value == '{') ++braces;
    else if (value == '}') --braces;
    else if (value == '[') ++brackets;
    else if (value == ']') --brackets;
    if (braces < 0 || brackets < 0) return false;
  }
  return !quoted && !escaped && braces == 0 && brackets == 0 &&
         !json.empty() && json.front() == '{' && json.back() == '\n';
}
}  // namespace

void test_schema_snapshot_views_revision_and_types() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  strcpy(snapshot.active.callsign, "IW2NNV");
  strcpy(snapshot.candidate.callsign, "IZ2ABC");
  strcpy(snapshot.saved.callsign, "AC2WZ");
  snapshot.candidate.frequencyMode = WsprFrequencyMode::RANDOM;
  snapshot.candidate.baseFrequencyHz = 14097189UL;
  snapshot.candidate.frequencyOffsetHz = -10;
  snapshot.candidate.si5351Correction = -76000;
  snapshot.candidate.powerDbm = 13;
  snapshot.candidate.transmitIntervalMinutes = 6;
  snapshot.status.wsprDirty = true;
  snapshot.activeDiffersSaved = true;

  const std::string json = serialize(snapshot);
  TEST_ASSERT_TRUE(structurallyValidJson(json));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"schema\":1,\"revision\":7"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"active\":{\"callsign\":\"IW2NNV\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"candidate\":{\"callsign\":\"IZ2ABC\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"saved\":{\"callsign\":\"AC2WZ\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"frequency_mode\":\"RANDOM\""));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"frequency_hz\":14097189"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"offset_hz\":-10"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        json.find("\"correction\":-76000"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"power_dbm\":13"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"interval_min\":6"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"power_save\":false"));
}

void test_state_distinguishes_candidate_active_saved_and_migration() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  snapshot.status.wsprDirty = true;
  snapshot.activeDiffersSaved = false;
  snapshot.status.wsprMigrationPending = true;
  snapshot.status.wsprPayloadVersion =
      PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V1;
  snapshot.runtimeSynchronized = false;
  const std::string json = serialize(snapshot);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("\"candidate_dirty\":true,\"active_differs_saved\":false"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("\"wspr_origin\":\"SAVED\",\"persistence_version\":1"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("\"migration_pending\":true,\"runtime_synchronized\":false"));
}

void test_default_state_uses_null_for_unavailable_persistence_version() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  snapshot.status.wsprOrigin = ConfigOrigin::DEFAULTS;
  snapshot.status.wsprPayloadVersion = 0;
  const std::string json = serialize(snapshot);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("\"wspr_origin\":\"DEFAULTS\",\"persistence_version\":null"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      json.find("\"frequency_mode\":\"FIXED\""));
}

void test_capabilities_come_from_band_plan_and_validator() {
  const std::string json = serialize(baseSnapshot());
  const size_t capabilities = json.find("\"capabilities\":");
  TEST_ASSERT_NOT_EQUAL(std::string::npos, capabilities);
  const std::string metadata = json.substr(capabilities);
  size_t previous = metadata.find("\"selectable_bands\":[");
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    const std::string entry =
        "{\"id\":" + std::to_string(static_cast<uint8_t>(band->id)) +
        ",\"name\":\"" + band->name +
        "\",\"tx_enabled\":true,\"nominal_frequency_hz\":" +
        std::to_string(band->nominalToneZeroHz) +
        ",\"frequency_min_hz\":" + std::to_string(band->lowerLimitHz) +
        ",\"frequency_max_hz\":" + std::to_string(band->upperLimitHz) +
        "}";
    const size_t found = metadata.find(entry, previous);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, found);
    TEST_ASSERT_TRUE(found >= previous);
    previous = found;
  }
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      metadata.find("\"frequency_modes\":[\"FIXED\",\"RANDOM\"]"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      metadata.find("\"power_dbm\":[0,3,7,10,13,17,20,23,27,30,33,37,40,43,47,50,53,57,60]"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      metadata.find("\"interval_min\":{\"min\":2,\"max\":60,\"step\":2}"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      metadata.find("\"offset_hz\":{\"min\":-100,\"max\":100}"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      metadata.find("\"correction\":{\"min\":-1000000,\"max\":1000000}"));
  TEST_ASSERT_EQUAL_UINT8(19, ConfigValidator::allowedPowerCount());
}

void test_strings_are_escaped_and_credentials_are_absent() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  strcpy(snapshot.candidate.callsign, "A\"\\\n");
  strcpy(snapshot.candidate.fixedLocator, "J\t45");
  const std::string json = serialize(snapshot);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("A\\\"\\\\\\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("J\\t45"));
  TEST_ASSERT_EQUAL(std::string::npos, json.find("password"));
  TEST_ASSERT_EQUAL(std::string::npos, json.find("ssid"));
  TEST_ASSERT_EQUAL(std::string::npos, json.find("wifi"));
  TEST_ASSERT_EQUAL(std::string::npos, json.find("eeprom"));
}

void test_serialization_does_not_mutate_snapshot() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  ConfigurationSnapshot before = snapshot;
  serialize(snapshot);
  TEST_ASSERT_EQUAL_MEMORY(&before, &snapshot, sizeof(snapshot));
}

void test_buffer_capacity_and_overflow_are_safe() {
  ConfigurationSnapshot snapshot = baseSnapshot();
  char maximum[ConfigurationJson::MAX_RESPONSE_BYTES + 1U];
  size_t length = 0;
  TEST_ASSERT_TRUE(ConfigurationJson::serialize(
      snapshot, maximum, sizeof(maximum), length));
  TEST_ASSERT_TRUE(length < ConfigurationJson::MAX_RESPONSE_BYTES);
  TEST_ASSERT_TRUE(length < 2049U);
  TEST_ASSERT_GREATER_THAN_UINT32(0, length);

  char small[128] = "stale";
  length = 99;
  TEST_ASSERT_FALSE(ConfigurationJson::serialize(
      snapshot, small, sizeof(small), length));
  TEST_ASSERT_EQUAL_UINT32(0, length);
  TEST_ASSERT_EQUAL_CHAR('\0', small[0]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_schema_snapshot_views_revision_and_types);
  RUN_TEST(test_state_distinguishes_candidate_active_saved_and_migration);
  RUN_TEST(test_default_state_uses_null_for_unavailable_persistence_version);
  RUN_TEST(test_capabilities_come_from_band_plan_and_validator);
  RUN_TEST(test_strings_are_escaped_and_credentials_are_absent);
  RUN_TEST(test_serialization_does_not_mutate_snapshot);
  RUN_TEST(test_buffer_capacity_and_overflow_are_safe);
  return UNITY_END();
}
