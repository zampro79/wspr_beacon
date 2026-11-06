#include <unity.h>

#include <string.h>

#include "BuildProfile.h"
#include "FirmwareIdentity.h"

namespace {
void test_identity_uses_central_project_version_profile_and_target() {
  TEST_ASSERT_EQUAL_STRING("WSPR Beacon 2.0", FirmwareIdentity::project());
  TEST_ASSERT_EQUAL_STRING("dev", FirmwareIdentity::version());
  TEST_ASSERT_EQUAL_STRING(WSPR_BUILD_PROFILE_NAME, FirmwareIdentity::profile());
  TEST_ASSERT_EQUAL_STRING("NATIVE", FirmwareIdentity::target());
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_EQUAL_STRING("DEVELOPMENT", FirmwareIdentity::profile());
#else
  TEST_ASSERT_EQUAL_STRING("PUBLIC", FirmwareIdentity::profile());
#endif
}

void test_commit_is_bounded_and_unknown_is_supported() {
  const char *commit = FirmwareIdentity::commit();
  TEST_ASSERT_NOT_NULL(commit);
  TEST_ASSERT_GREATER_THAN_UINT32(0, strlen(commit));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(40, strlen(commit));
}

void test_serial_identity_is_concise_and_fails_closed_on_small_buffer() {
  char line[96];
  TEST_ASSERT_TRUE(FirmwareIdentity::format(line, sizeof(line)));
  TEST_ASSERT_NOT_NULL(strstr(line, "WSPR Beacon 2.0 | "));
  TEST_ASSERT_NOT_NULL(strstr(line, " | NATIVE | dev | commit "));
  TEST_ASSERT_NULL(strchr(line, '\n'));
  TEST_ASSERT_NULL(strstr(line, "/home/"));

  char small[8] = "changed";
  TEST_ASSERT_FALSE(FirmwareIdentity::format(small, sizeof(small)));
  TEST_ASSERT_EQUAL_STRING("", small);
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_identity_uses_central_project_version_profile_and_target);
  RUN_TEST(test_commit_is_bounded_and_unknown_is_supported);
  RUN_TEST(test_serial_identity_is_concise_and_fails_closed_on_small_buffer);
  return UNITY_END();
}
