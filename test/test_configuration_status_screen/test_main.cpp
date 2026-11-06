#include <string.h>
#include <unity.h>

#include "ConfigurationStatusScreen.h"

void test_cli_web_and_both_configuration_pages() {
  ConfigurationSessionSnapshot session;
  session.mode = FirmwareOperatingMode::CONFIGURATION;
  session.cliActive = true;
  ConfigurationStatusSnapshot screen = ConfigurationStatusScreen::build(session);
  TEST_ASSERT_EQUAL_STRING("CONFIGURATION", screen.lines[0]);
  TEST_ASSERT_EQUAL_STRING("TX DISABLED", screen.lines[1]);
  TEST_ASSERT_EQUAL_STRING("SESSION: CLI", screen.lines[2]);

  session.cliActive = false;
  session.webActive = true;
  screen = ConfigurationStatusScreen::build(session);
  TEST_ASSERT_EQUAL_STRING("SESSION: WEB", screen.lines[2]);

  session.cliActive = true;
  screen = ConfigurationStatusScreen::build(session);
  TEST_ASSERT_EQUAL_STRING("SESSION: BOTH", screen.lines[2]);
}

void test_resume_guard_page_and_rf_fault_priority() {
  ConfigurationSessionSnapshot session;
  session.resumeGuardActive = true;
  ConfigurationStatusSnapshot screen = ConfigurationStatusScreen::build(session);
  TEST_ASSERT_EQUAL_STRING("TX RESUME GUARD", screen.lines[0]);
  TEST_ASSERT_EQUAL_STRING("TX DISABLED", screen.lines[1]);
  TEST_ASSERT_EQUAL_STRING("WAITING SAFE SLOT", screen.lines[2]);
  TEST_ASSERT_TRUE(ConfigurationStatusScreen::visible(session,
                                                      OledRfState::READY));
  TEST_ASSERT_FALSE(ConfigurationStatusScreen::visible(session,
                                                       OledRfState::ERROR));
  TEST_ASSERT_FALSE(ConfigurationStatusScreen::visible(
      session, OledRfState::INHIBITED));
}

void test_normal_mode_has_no_configuration_page_and_all_text_fits() {
  ConfigurationSessionSnapshot session;
  TEST_ASSERT_FALSE(ConfigurationStatusScreen::visible(session,
                                                       OledRfState::READY));
  session.mode = FirmwareOperatingMode::CONFIGURATION;
  session.cliActive = true;
  session.webActive = true;
  const ConfigurationStatusSnapshot screen =
      ConfigurationStatusScreen::build(session);
  for (uint8_t index = 0; index < ConfigurationStatusSnapshot::LINE_COUNT;
       ++index) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(
        ConfigurationStatusSnapshot::MAX_CHARACTERS,
        static_cast<uint8_t>(strlen(screen.lines[index])));
  }
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_cli_web_and_both_configuration_pages);
  RUN_TEST(test_resume_guard_page_and_rf_fault_priority);
  RUN_TEST(test_normal_mode_has_no_configuration_page_and_all_text_fits);
  return UNITY_END();
}
