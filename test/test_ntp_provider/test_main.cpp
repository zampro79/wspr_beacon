#include <unity.h>

#include "NtpProviderCore.h"
#include "NtpSettings.h"

namespace {
constexpr NtpProviderConfig CONFIG(1000UL, 5000UL, 3000UL, 6000UL);

void test_disabled_provider_never_requests_sync() {
  NtpProviderCore provider(CONFIG);
  provider.begin(false);
  provider.setWifiConnected(true);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_DISABLED),
                    static_cast<int>(provider.snapshot(0).state));
}

void test_wifi_connection_allows_non_blocking_request() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_WAITING_FOR_WIFI),
                    static_cast<int>(provider.snapshot(10).state));
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.requestNeeded());
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  const NtpProviderSnapshot snapshot = provider.snapshot(10);
  TEST_ASSERT_TRUE(snapshot.wifiConnected);
  TEST_ASSERT_TRUE(snapshot.requestStarted);
  TEST_ASSERT_FALSE(snapshot.syncConfirmed);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_REQUESTING),
                    static_cast<int>(snapshot.state));
}

void test_sync_requires_explicit_confirmation() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.update(500);
  TEST_ASSERT_FALSE(provider.snapshot(500).syncConfirmed);
  provider.markSynchronized(600);
  const NtpProviderSnapshot snapshot = provider.snapshot(700);
  TEST_ASSERT_TRUE(snapshot.syncConfirmed);
  TEST_ASSERT_EQUAL_UINT32(100, snapshot.lastSyncAgeMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(snapshot.state));
}

void test_request_timeout_and_late_recovery() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.update(1011);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_TIMED_OUT),
                    static_cast<int>(provider.snapshot(1011).state));
  provider.markSynchronized(1200);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(provider.snapshot(1200).state));
}

void test_timeout_waits_for_watchdog_before_controlled_retry() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.update(1011);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(3009);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(3010);
  TEST_ASSERT_TRUE(provider.requestNeeded());
  TEST_ASSERT_TRUE(provider.markRequestStarted(3010));
  TEST_ASSERT_FALSE(provider.requestNeeded());
  TEST_ASSERT_FALSE(provider.markRequestStarted(3010));
}

void test_recovery_backoff_is_monotonic_and_bounded() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(0));
  provider.update(3000);
  TEST_ASSERT_TRUE(provider.markRequestStarted(3000));
  provider.update(8999);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(9000);
  TEST_ASSERT_TRUE(provider.markRequestStarted(9000));
  provider.update(14999);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(15000);
  TEST_ASSERT_TRUE(provider.markRequestStarted(15000));
  provider.update(20999);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(21000);
  TEST_ASSERT_TRUE(provider.requestNeeded());
}

void test_valid_callback_resets_recovery_backoff() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(0));
  provider.update(3000);
  TEST_ASSERT_TRUE(provider.markRequestStarted(3000));
  provider.markSynchronized(3500);
  provider.update(8501);
  TEST_ASSERT_TRUE(provider.requestNeeded());
  TEST_ASSERT_TRUE(provider.markRequestStarted(8501));
  provider.update(11500);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.update(11501);
  TEST_ASSERT_TRUE(provider.requestNeeded());
}

void test_reference_becomes_stale() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.markSynchronized(100);
  provider.update(5100);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(provider.snapshot(5100).state));
  provider.update(5101);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_STALE),
                    static_cast<int>(provider.snapshot(5101).state));
  TEST_ASSERT_TRUE(provider.requestNeeded());
  TEST_ASSERT_TRUE(provider.markRequestStarted(5101));
  TEST_ASSERT_FALSE(provider.snapshot(5101).syncConfirmed);
  provider.markSynchronized(5200);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(provider.snapshot(5200).state));
}

void test_repeated_valid_sync_refreshes_reference_age() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.markSynchronized(100);
  provider.markSynchronized(4000);
  provider.update(6000);
  TEST_ASSERT_EQUAL_UINT32(2000, provider.snapshot(6000).lastSyncAgeMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(provider.snapshot(6000).state));
}

void test_esp32_sync_policy_precedes_reference_expiry() {
  TEST_ASSERT_TRUE(ESP32_SNTP_SYNC_INTERVAL_MS < NTP_REFERENCE_TIMEOUT_MS);
  TEST_ASSERT_TRUE(ESP32_NTP_RECOVERY_INITIAL_MS > NTP_REQUEST_TIMEOUT_MS);
  TEST_ASSERT_TRUE(ESP32_NTP_RECOVERY_INITIAL_MS <=
                   ESP32_NTP_RECOVERY_MAX_MS);
}

void test_disabled_watchdog_preserves_esp8266_provider_behavior() {
  NtpProviderCore provider(NtpProviderConfig(1000UL, 5000UL));
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(0));
  provider.update(6000);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_TIMED_OUT),
                    static_cast<int>(provider.snapshot(6000).state));
  TEST_ASSERT_FALSE(provider.requestNeeded());
}

void test_connection_loss_before_sync_returns_to_waiting() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.setWifiConnected(false);
  const NtpProviderSnapshot snapshot = provider.snapshot(20);
  TEST_ASSERT_FALSE(snapshot.wifiConnected);
  TEST_ASSERT_FALSE(snapshot.requestStarted);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_WAITING_FOR_WIFI),
                    static_cast<int>(snapshot.state));
}

void test_reconnect_starts_one_request_without_concurrent_duplicate() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.setWifiConnected(false);
  TEST_ASSERT_FALSE(provider.requestNeeded());
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.requestNeeded());
  TEST_ASSERT_TRUE(provider.markRequestStarted(20));
  TEST_ASSERT_FALSE(provider.requestNeeded());
  TEST_ASSERT_FALSE(provider.markRequestStarted(20));
}

void test_connection_loss_does_not_immediately_discard_reference() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(10));
  provider.markSynchronized(100);
  provider.setWifiConnected(false);
  provider.update(200);
  const NtpProviderSnapshot snapshot = provider.snapshot(200);
  TEST_ASSERT_FALSE(snapshot.wifiConnected);
  TEST_ASSERT_TRUE(snapshot.syncConfirmed);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(snapshot.state));
}

void test_monotonic_overflow_preserves_age_and_timeouts() {
  NtpProviderCore provider(CONFIG);
  provider.begin(true);
  provider.setWifiConnected(true);
  TEST_ASSERT_TRUE(provider.markRequestStarted(UINT32_MAX - 100UL));
  provider.markSynchronized(UINT32_MAX - 50UL);
  provider.update(49);
  TEST_ASSERT_EQUAL_UINT32(100, provider.snapshot(49).lastSyncAgeMillis);
  TEST_ASSERT_EQUAL(static_cast<int>(NtpProviderState::STATE_SYNCHRONIZED),
                    static_cast<int>(provider.snapshot(49).state));
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_disabled_provider_never_requests_sync);
  RUN_TEST(test_wifi_connection_allows_non_blocking_request);
  RUN_TEST(test_sync_requires_explicit_confirmation);
  RUN_TEST(test_request_timeout_and_late_recovery);
  RUN_TEST(test_timeout_waits_for_watchdog_before_controlled_retry);
  RUN_TEST(test_recovery_backoff_is_monotonic_and_bounded);
  RUN_TEST(test_valid_callback_resets_recovery_backoff);
  RUN_TEST(test_reference_becomes_stale);
  RUN_TEST(test_repeated_valid_sync_refreshes_reference_age);
  RUN_TEST(test_esp32_sync_policy_precedes_reference_expiry);
  RUN_TEST(test_disabled_watchdog_preserves_esp8266_provider_behavior);
  RUN_TEST(test_connection_loss_before_sync_returns_to_waiting);
  RUN_TEST(test_reconnect_starts_one_request_without_concurrent_duplicate);
  RUN_TEST(test_connection_loss_does_not_immediately_discard_reference);
  RUN_TEST(test_monotonic_overflow_preserves_age_and_timeouts);
  return UNITY_END();
}
