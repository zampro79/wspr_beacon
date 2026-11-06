#include <unity.h>

#include <string.h>

#include "LocatorService.h"

namespace {
void assertLocator(const LocatorSnapshot &snapshot, const char *locator,
                   LocatorSource source) {
  TEST_ASSERT_EQUAL_STRING(locator, snapshot.locator);
  TEST_ASSERT_EQUAL(static_cast<int>(source),
                    static_cast<int>(snapshot.source));
  TEST_ASSERT_EQUAL_UINT32(4, strlen(snapshot.locator));
}

void test_startup_without_fix_uses_configured_fixed_locator() {
  LocatorService service;
  service.update("JN45", false, "----");
  assertLocator(service.snapshot(), "JN45", LocatorSource::FIXED);
}

void test_first_fix_becomes_live_and_loss_keeps_last_known() {
  LocatorService service;
  service.update("JN45", true, "JN58");
  assertLocator(service.snapshot(), "JN58", LocatorSource::GPS_LIVE);
  service.update("JN45", false, "----");
  assertLocator(service.snapshot(), "JN58", LocatorSource::GPS_LAST_KNOWN);
}

void test_recovery_and_changed_position_update_locator() {
  LocatorService service;
  service.update("JN45", true, "JN58");
  service.update("JN45", false, "----");
  service.update("JN45", true, "JN57");
  assertLocator(service.snapshot(), "JN57", LocatorSource::GPS_LIVE);
}

void test_fixed_configuration_changes_before_first_fix() {
  LocatorService service;
  service.update("JN45", false, "----");
  service.update("IO91", false, "----");
  assertLocator(service.snapshot(), "IO91", LocatorSource::FIXED);
}

void test_invalid_or_six_character_locator_is_never_accepted() {
  LocatorService service;
  TEST_ASSERT_TRUE(LocatorService::validMaidenhead4("JN45"));
  TEST_ASSERT_FALSE(LocatorService::validMaidenhead4("JN45AB"));
  TEST_ASSERT_FALSE(LocatorService::validMaidenhead4("SZ99"));
  service.update("JN45", true, "JN45AB");
  assertLocator(service.snapshot(), "JN45", LocatorSource::FIXED);
}

void test_frame_snapshot_is_immutable_after_locator_changes() {
  LocatorService service;
  service.update("JN45", true, "JN58");
  const LocatorSnapshot preparedFrame = service.snapshot();
  service.update("JN45", true, "JN57");
  assertLocator(preparedFrame, "JN58", LocatorSource::GPS_LIVE);
  assertLocator(service.snapshot(), "JN57", LocatorSource::GPS_LIVE);
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_startup_without_fix_uses_configured_fixed_locator);
  RUN_TEST(test_first_fix_becomes_live_and_loss_keeps_last_known);
  RUN_TEST(test_recovery_and_changed_position_update_locator);
  RUN_TEST(test_fixed_configuration_changes_before_first_fix);
  RUN_TEST(test_invalid_or_six_character_locator_is_never_accepted);
  RUN_TEST(test_frame_snapshot_is_immutable_after_locator_changes);
  return UNITY_END();
}
