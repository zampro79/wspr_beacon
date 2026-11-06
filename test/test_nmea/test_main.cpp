#include <math.h>
#include <string.h>

#include <unity.h>

#include "Maidenhead.h"
#include "NmeaParser.h"

static bool parseSentence(const char *source, NmeaUpdate &update) {
  char sentence[NmeaLineBuffer::CAPACITY];
  strncpy(sentence, source, sizeof(sentence) - 1);
  sentence[sizeof(sentence) - 1] = '\0';
  return NmeaParser::parse(sentence, update);
}

static void test_gga_regression() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", update));
  TEST_ASSERT_EQUAL(static_cast<int>(NmeaSentenceType::GGA), static_cast<int>(update.type));
  TEST_ASSERT_TRUE(update.timeValid);
  TEST_ASSERT_EQUAL_UINT8(12, update.hour);
  TEST_ASSERT_EQUAL_UINT8(35, update.minute);
  TEST_ASSERT_EQUAL_UINT8(19, update.second);
  TEST_ASSERT_TRUE(update.positionValid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 48.1173f, update.latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 11.516666f, update.longitude);
  TEST_ASSERT_EQUAL_UINT8(8, update.satellites);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 545.4f, update.altitude);
}

static void test_rmc_regression() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A", update));
  TEST_ASSERT_EQUAL(static_cast<int>(NmeaSentenceType::RMC), static_cast<int>(update.type));
  TEST_ASSERT_TRUE(update.timeValid);
  TEST_ASSERT_TRUE(update.positionValid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 48.1173f, update.latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 11.516666f, update.longitude);
}

static void test_gn_talker_and_dynamic_locator() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GNGGA,225444,4510.6601,N,00939.4374,E,1,07,1.2,120.5,M,0.0,M,,*69", update));
  TEST_ASSERT_TRUE(update.positionValid);
  char locator[5];
  TEST_ASSERT_TRUE(maidenhead4(update.latitude, update.longitude, locator));
  TEST_ASSERT_EQUAL_STRING("JN45", locator);
}

static void test_south_west_coordinates() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GNRMC,225445,A,3450.000,S,05822.000,W,0.0,0.0,010100,,,A*6E", update));
  TEST_ASSERT_TRUE(update.positionValid);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -34.833333f, update.latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -58.366667f, update.longitude);
}

static void test_no_fix_preserves_separate_updates() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GPGGA,120000,4807.038,N,01131.000,E,0,03,2.0,10.0,M,0.0,M,,*42", update));
  TEST_ASSERT_TRUE(update.timeValid);
  TEST_ASSERT_FALSE(update.positionValid);
  TEST_ASSERT_EQUAL_UINT8(3, update.satellites);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, update.altitude);
}

static void test_leap_second_is_clamped() {
  NmeaUpdate update;
  TEST_ASSERT_TRUE(parseSentence(
      "$GPRMC,235960,A,0000.000,N,00000.000,E,0,0,010100,,,A*7B", update));
  TEST_ASSERT_TRUE(update.timeValid);
  TEST_ASSERT_EQUAL_UINT8(59, update.second);
}

static void test_invalid_data_is_rejected_or_flagged() {
  NmeaUpdate update;
  TEST_ASSERT_FALSE(parseSentence(
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00", update));
  TEST_ASSERT_FALSE(parseSentence(
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", update));
  TEST_ASSERT_TRUE(parseSentence(
      "$GPGGA,246000,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4A", update));
  TEST_ASSERT_FALSE(update.timeValid);
  TEST_ASSERT_TRUE(parseSentence(
      "$GPGGA,120000,4860.000,N,01131.000,E,1,08,0.9,1.0,M,0.0,M,,*79", update));
  TEST_ASSERT_FALSE(update.positionValid);
}

static void test_line_buffer_framing_and_recovery() {
  NmeaLineBuffer buffer;
  TEST_ASSERT_FALSE(buffer.push('x'));
  const char *sentence = "$GPRMC,1*00\r\n";
  bool ready = false;
  for (const char *cursor = sentence; *cursor; ++cursor) ready = buffer.push(*cursor) || ready;
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_EQUAL_STRING("$GPRMC,1*00", buffer.sentence());

  buffer.reset();
  TEST_ASSERT_FALSE(buffer.push('$'));
  for (size_t i = 0; i < NmeaLineBuffer::CAPACITY; ++i) TEST_ASSERT_FALSE(buffer.push('A'));
  TEST_ASSERT_FALSE(buffer.push('\n'));
  TEST_ASSERT_FALSE(buffer.push('$'));
  TEST_ASSERT_FALSE(buffer.push('X'));
  TEST_ASSERT_TRUE(buffer.push('\n'));
  TEST_ASSERT_EQUAL_STRING("$X", buffer.sentence());
}

static void test_maidenhead_known_points_and_bounds() {
  char locator[5] = {};
  TEST_ASSERT_TRUE(maidenhead4(0.0f, 0.0f, locator));
  TEST_ASSERT_EQUAL_STRING("JJ00", locator);
  TEST_ASSERT_TRUE(maidenhead4(48.1173f, 11.516666f, locator));
  TEST_ASSERT_EQUAL_STRING("JN58", locator);
  TEST_ASSERT_FALSE(maidenhead4(90.1f, 0.0f, locator));
  TEST_ASSERT_FALSE(maidenhead4(0.0f, -180.1f, locator));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_gga_regression);
  RUN_TEST(test_rmc_regression);
  RUN_TEST(test_gn_talker_and_dynamic_locator);
  RUN_TEST(test_south_west_coordinates);
  RUN_TEST(test_no_fix_preserves_separate_updates);
  RUN_TEST(test_leap_second_is_clamped);
  RUN_TEST(test_invalid_data_is_rejected_or_flagged);
  RUN_TEST(test_line_buffer_framing_and_recovery);
  RUN_TEST(test_maidenhead_known_points_and_bounds);
  return UNITY_END();
}
