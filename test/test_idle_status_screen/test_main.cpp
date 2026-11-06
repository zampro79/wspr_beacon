#include <string.h>
#include <unity.h>

#include "IdleStatusScreen.h"

namespace {
IdleStatusInputs defaults() {
  IdleStatusInputs inputs;
  inputs.callsign = "IW2NNV";
  inputs.locator = "JN45";
  inputs.band = "20m";
  inputs.frequencyHz = 14097100UL;
  inputs.powerDbm = 23;
  inputs.rfState = OledRfState::READY;
  inputs.time.statusFlags = 1U;
  inputs.time.utcMillisOfDay = 45318000UL;
  inputs.nextSlotUtcMillis = 45360000UL;
  inputs.nextSlotValid = true;
  return inputs;
}

void assertFits(const IdleStatusSnapshot &snapshot) {
  for (uint8_t index = 0; index < IdleStatusSnapshot::LINE_COUNT; ++index) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(
        IdleStatusSnapshot::MAX_CHARACTERS,
        static_cast<uint8_t>(strlen(snapshot.lines[index])));
  }
}
}  // namespace

void test_idle_layout_uses_actual_message_frequency_and_slot() {
  const IdleStatusSnapshot snapshot = IdleStatusScreen::build(defaults());
  TEST_ASSERT_EQUAL_STRING("MSG: IW2NNV JN45 23", snapshot.lines[0]);
  TEST_ASSERT_EQUAL_STRING("FREQ: 14.097100 (20m)", snapshot.lines[1]);
  TEST_ASSERT_EQUAL_STRING("RF: READY", snapshot.lines[2]);
  TEST_ASSERT_EQUAL_STRING("NEXT TX: 12:36 UTC", snapshot.lines[3]);
  TEST_ASSERT_EQUAL_STRING("REMAINING: 42s", snapshot.lines[4]);
  assertFits(snapshot);
}

void test_random_mode_never_presents_previous_frame_frequency_as_next() {
  IdleStatusInputs inputs = defaults();
  inputs.frequencyMode = WsprFrequencyMode::RANDOM;
  inputs.frequencyHz = 14097189UL;
  const IdleStatusSnapshot snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("FREQ: RANDOM (20m)", snapshot.lines[1]);
  TEST_ASSERT_NULL(strstr(snapshot.lines[1], "14.097189"));
  assertFits(snapshot);
}

void test_maximum_lengths_fit_128_pixels() {
  IdleStatusInputs inputs = defaults();
  inputs.callsign = "ABCDEF";
  inputs.locator = "RR99";
  inputs.powerDbm = 60;
  inputs.frequencyHz = 28126100UL;
  inputs.band = "10m";
  inputs.time.utcMillisOfDay = 0;
  inputs.nextSlotUtcMillis = 3600000UL;
  const IdleStatusSnapshot snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("MSG: ABCDEF RR99 60", snapshot.lines[0]);
  TEST_ASSERT_EQUAL_STRING("FREQ: 28.126100 (10m)", snapshot.lines[1]);
  TEST_ASSERT_EQUAL_STRING("REMAINING: 3600s", snapshot.lines[4]);
  assertFits(snapshot);
}

void test_rf_states_are_distinct() {
  IdleStatusInputs inputs = defaults();
  inputs.rfState = OledRfState::INHIBITED;
  TEST_ASSERT_EQUAL_STRING("RF: INHIBITED",
                           IdleStatusScreen::build(inputs).lines[2]);
  inputs.rfState = OledRfState::ERROR;
  TEST_ASSERT_EQUAL_STRING("RF: ERROR",
                           IdleStatusScreen::build(inputs).lines[2]);
}

void test_waiting_for_time_is_distinct_from_other_rf_states() {
  IdleStatusInputs inputs;
  inputs.rfState = OledRfState::WAITING_TIME;
  const IdleStatusSnapshot snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("RF: WAIT TIME", snapshot.lines[2]);
}

void test_unconfigured_identity_has_dedicated_oled_label() {
  IdleStatusInputs inputs = defaults();
  inputs.callsign = "";
  TEST_ASSERT_EQUAL_STRING("NO CALLSIGN",
                           IdleStatusScreen::build(inputs).lines[0]);
}

void test_countdown_rounds_up_and_handles_slot_change_at_midnight() {
  IdleStatusInputs inputs = defaults();
  inputs.time.utcMillisOfDay = 45359501UL;
  TEST_ASSERT_EQUAL_STRING("REMAINING: 1s",
                           IdleStatusScreen::build(inputs).lines[4]);
  inputs.time.utcMillisOfDay = 86399001UL;
  inputs.nextSlotUtcMillis = 0;
  const IdleStatusSnapshot snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("NEXT TX: 00:00 UTC", snapshot.lines[3]);
  TEST_ASSERT_EQUAL_STRING("REMAINING: 1s", snapshot.lines[4]);
}

void test_unknown_slot_never_shows_stale_countdown() {
  IdleStatusInputs inputs = defaults();
  inputs.nextSlotValid = false;
  IdleStatusSnapshot snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("NEXT TX: --:-- UTC", snapshot.lines[3]);
  TEST_ASSERT_EQUAL_STRING("REMAINING: --", snapshot.lines[4]);
  inputs.nextSlotValid = true;
  inputs.time.statusFlags = 0;
  snapshot = IdleStatusScreen::build(inputs);
  TEST_ASSERT_EQUAL_STRING("REMAINING: --", snapshot.lines[4]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_layout_uses_actual_message_frequency_and_slot);
  RUN_TEST(test_random_mode_never_presents_previous_frame_frequency_as_next);
  RUN_TEST(test_maximum_lengths_fit_128_pixels);
  RUN_TEST(test_rf_states_are_distinct);
  RUN_TEST(test_waiting_for_time_is_distinct_from_other_rf_states);
  RUN_TEST(test_unconfigured_identity_has_dedicated_oled_label);
  RUN_TEST(test_countdown_rounds_up_and_handles_slot_change_at_midnight);
  RUN_TEST(test_unknown_slot_never_shows_stale_countdown);
  return UNITY_END();
}
