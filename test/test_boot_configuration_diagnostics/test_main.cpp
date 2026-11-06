#include <unity.h>

#include "BootConfigurationDiagnostics.h"

namespace {
void assertWarning(BootConfigurationWarning expected,
                   BootConfigurationDomain domain, bool rejected,
                   ConfigOrigin origin, FactoryResetBootState factoryState) {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(expected),
      static_cast<uint8_t>(BootConfigurationDiagnostics::classify(
          domain, rejected, origin, factoryState)));
}
}  // namespace

void test_no_rejected_record_emits_nothing_for_both_domains() {
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WSPR, false, ConfigOrigin::SAVED,
                FactoryResetBootState::NORMAL);
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WIFI, false, ConfigOrigin::SAVED,
                FactoryResetBootState::NORMAL);
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WSPR, false, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::NORMAL);
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WIFI, false, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::NORMAL);
}

void test_saved_recovery_names_rejected_slot_for_each_domain() {
  assertWarning(BootConfigurationWarning::WSPR_SLOT_REJECTED_RECOVERED_SAVED,
                BootConfigurationDomain::WSPR, true, ConfigOrigin::SAVED,
                FactoryResetBootState::NORMAL);
  assertWarning(BootConfigurationWarning::WIFI_SLOT_REJECTED_RECOVERED_SAVED,
                BootConfigurationDomain::WIFI, true, ConfigOrigin::SAVED,
                FactoryResetBootState::NORMAL);
}

void test_default_fallback_names_absence_of_valid_record() {
  assertWarning(BootConfigurationWarning::WSPR_NO_VALID_RECORD_USING_DEFAULTS,
                BootConfigurationDomain::WSPR, true, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::NORMAL);
  assertWarning(BootConfigurationWarning::WIFI_NO_VALID_RECORD_USING_DEFAULTS,
                BootConfigurationDomain::WIFI, true, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::NORMAL);
}

void test_completed_and_recovered_factory_states_keep_recovery_detail() {
  assertWarning(BootConfigurationWarning::WIFI_SLOT_REJECTED_RECOVERED_SAVED,
                BootConfigurationDomain::WIFI, true, ConfigOrigin::SAVED,
                FactoryResetBootState::COMPLETED);
  assertWarning(BootConfigurationWarning::WIFI_NO_VALID_RECORD_USING_DEFAULTS,
                BootConfigurationDomain::WIFI, true, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::RECOVERED);
}

void test_fail_closed_never_reports_ordinary_default_fallback() {
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WSPR, true, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::FAIL_CLOSED);
  assertWarning(BootConfigurationWarning::NONE,
                BootConfigurationDomain::WIFI, true, ConfigOrigin::DEFAULTS,
                FactoryResetBootState::FAIL_CLOSED);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_rejected_record_emits_nothing_for_both_domains);
  RUN_TEST(test_saved_recovery_names_rejected_slot_for_each_domain);
  RUN_TEST(test_default_fallback_names_absence_of_valid_record);
  RUN_TEST(test_completed_and_recovered_factory_states_keep_recovery_detail);
  RUN_TEST(test_fail_closed_never_reports_ordinary_default_fallback);
  return UNITY_END();
}
