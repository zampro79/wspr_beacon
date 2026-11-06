#include <string.h>
#include <unity.h>

#include "OperationalStatusScreen.h"

namespace {
OperationalStatusInputs defaults(uint32_t nowMillis = 0) {
  OperationalStatusInputs inputs;
  inputs.nowMillis = nowMillis;
  inputs.locator.source = LocatorSource::FIXED;
  memcpy(inputs.locator.locator, "JN45", 5);
  return inputs;
}

void assertFits(const OperationalStatusSnapshot &snapshot) {
  for (uint8_t index = 0; index < OperationalStatusSnapshot::LINE_COUNT;
       ++index) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(
        OperationalStatusSnapshot::MAX_CHARACTERS,
        static_cast<uint8_t>(strlen(snapshot.lines[index])));
  }
}
}  // namespace

void test_search_and_missing_values_are_explicit() {
  OperationalStatusScreen screen;
  const OperationalStatusSnapshot snapshot = screen.update(defaults());
  TEST_ASSERT_EQUAL_STRING("GPS: SEARCH SAT:--", snapshot.lines[0]);
  TEST_ASSERT_EQUAL_STRING("UTC: --:--:-- --- -", snapshot.lines[1]);
  TEST_ASSERT_EQUAL_STRING("COORD: NOT AVAILABLE", snapshot.lines[2]);
  TEST_ASSERT_EQUAL_STRING("LOC: JN45 (DEF)", snapshot.lines[3]);
  TEST_ASSERT_EQUAL_STRING("WiFi: OFF", snapshot.lines[4]);
  assertFits(snapshot);
}

void test_fix_and_negative_coordinates_use_real_snapshots() {
  OperationalStatusScreen screen;
  OperationalStatusInputs inputs = defaults();
  inputs.gpsNmeaAvailable = true;
  inputs.gpsFixValid = true;
  inputs.satellites = 8;
  inputs.latitude = -45.464F;
  inputs.longitude = -9.190F;
  inputs.locator.source = LocatorSource::GPS_LIVE;
  memcpy(inputs.locator.locator, "JN45", 5);
  const OperationalStatusSnapshot snapshot = screen.update(inputs);
  TEST_ASSERT_EQUAL_STRING("GPS: FIX SAT:08", snapshot.lines[0]);
  TEST_ASSERT_EQUAL_STRING("45.464S 009.190W", snapshot.lines[2]);
  TEST_ASSERT_EQUAL_STRING("LOC: JN45 (GPS)", snapshot.lines[3]);
  assertFits(snapshot);
}

void test_lost_fix_keeps_coordinates_and_marks_them_old() {
  OperationalStatusScreen screen;
  OperationalStatusInputs inputs = defaults();
  inputs.gpsNmeaAvailable = true;
  inputs.latitude = 90.0F;
  inputs.longitude = 180.0F;
  inputs.locator.source = LocatorSource::GPS_LAST_KNOWN;
  memcpy(inputs.locator.locator, "RR99", 5);
  const OperationalStatusSnapshot snapshot = screen.update(inputs);
  TEST_ASSERT_EQUAL_STRING("GPS: LOST SAT:00", snapshot.lines[0]);
  TEST_ASSERT_EQUAL_STRING("90.000N 180.000E OLD", snapshot.lines[2]);
  TEST_ASSERT_EQUAL_STRING("LOC: RR99 (LAST)", snapshot.lines[3]);
  assertFits(snapshot);
}

void test_selected_time_source_and_existing_authorization_are_rendered() {
  OperationalStatusScreen screen;
  OperationalStatusInputs inputs = defaults();
  inputs.time.statusFlags = 1U;
  inputs.time.utcMillisOfDay = 45296000UL;
  inputs.time.source = TimeSource::GPS;
  inputs.authorization = WsprTimeAuthorization::GPS_PRECISE;
  TEST_ASSERT_EQUAL_STRING("UTC: 12:34:56 GPS *",
                           screen.update(inputs).lines[1]);

  inputs.time.source = TimeSource::NTP;
  inputs.authorization = WsprTimeAuthorization::NTP_EXPERIMENTAL;
  TEST_ASSERT_EQUAL_STRING("UTC: 12:34:56 NTP *",
                           screen.update(inputs).lines[1]);
  inputs.authorization = WsprTimeAuthorization::NONE;
  TEST_ASSERT_EQUAL_STRING("UTC: 12:34:56 NTP !",
                           screen.update(inputs).lines[1]);
}

void test_wifi_connected_label_then_ipv4_and_disconnect_clears_it() {
  OperationalStatusScreen screen;
  OperationalStatusInputs inputs = defaults(1000UL);
  inputs.wifiState = WifiConnectionState::STATE_CONNECTED;
  inputs.wifiMode = WifiDisplayMode::STA;
  inputs.wifiIpv4 = "255.255.255.255";
  TEST_ASSERT_EQUAL_STRING("WiFi: CONNECTED (STA)",
                           screen.update(inputs).lines[4]);
  inputs.nowMillis = 3999UL;
  TEST_ASSERT_EQUAL_STRING("WiFi: CONNECTED (STA)",
                           screen.update(inputs).lines[4]);
  inputs.nowMillis = 4000UL;
  TEST_ASSERT_EQUAL_STRING("255.255.255.255(STA)",
                           screen.update(inputs).lines[4]);

  inputs.wifiState = WifiConnectionState::STATE_DISCONNECTED;
  inputs.wifiIpv4 = nullptr;
  TEST_ASSERT_EQUAL_STRING("W:DISCONNECTED (STA)",
                           screen.update(inputs).lines[4]);
  inputs.wifiState = WifiConnectionState::STATE_CONNECTING;
  inputs.wifiReconnecting = true;
  TEST_ASSERT_EQUAL_STRING("W:RECONNECTING (STA)",
                           screen.update(inputs).lines[4]);
}

void test_wifi_timer_is_wrap_safe_and_short_ipv4_keeps_label() {
  OperationalStatusScreen screen;
  OperationalStatusInputs inputs = defaults(UINT32_MAX - 99UL);
  inputs.wifiState = WifiConnectionState::STATE_CONNECTED;
  inputs.wifiMode = WifiDisplayMode::STA;
  inputs.wifiIpv4 = "1.2.3.4";
  screen.update(inputs);
  inputs.nowMillis = 2900UL;
  const OperationalStatusSnapshot snapshot = screen.update(inputs);
  TEST_ASSERT_EQUAL_STRING("WiFi: 1.2.3.4 (STA)", snapshot.lines[4]);
  assertFits(snapshot);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_search_and_missing_values_are_explicit);
  RUN_TEST(test_fix_and_negative_coordinates_use_real_snapshots);
  RUN_TEST(test_lost_fix_keeps_coordinates_and_marks_them_old);
  RUN_TEST(test_selected_time_source_and_existing_authorization_are_rendered);
  RUN_TEST(test_wifi_connected_label_then_ipv4_and_disconnect_clears_it);
  RUN_TEST(test_wifi_timer_is_wrap_safe_and_short_ipv4_keeps_label);
  return UNITY_END();
}
