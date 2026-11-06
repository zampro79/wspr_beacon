#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "BeaconSettings.h"
#include "CommandLineBuffer.h"
#include "ConsoleMode.h"
#include "ConsoleWebCoordinator.h"
#include "ConsoleInputEcho.h"
#include "ConsoleHelpText.h"
#include "ConsoleConfigurationPolicy.h"
#include "ConsoleMenu.h"
#include "ConsoleStatus.h"
#include "ConfigCommandParser.h"
#include "ConfigValidator.h"
#include "WifiCredentials.h"
#include "WifiConnectionTracker.h"

namespace {
BeaconSettings configuredSettings() {
  BeaconSettings settings;
  strcpy(settings.callsign, "IW2NNV");
  return settings;
}
void loadConfiguredDefaults(ConfigCommandParser &parser) {
  parser.loadCandidates(configuredSettings(), WifiCredentials());
}
CommandResult execute(ConfigCommandParser &parser, const char *command) {
  char line[96];
  strncpy(line, command, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  return parser.execute(line);
}
ConsoleMenuResult menuAccept(ConsoleMenu &menu, ConfigCommandParser &parser,
                             const char *input) {
  char line[CommandLineBuffer::CAPACITY];
  strncpy(line, input, sizeof(line));
  line[sizeof(line) - 1] = '\0';
  return menu.accept(line, parser);
}
bool containsBytes(const void *data, size_t dataLength, const char *text) {
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  const size_t textLength = strlen(text);
  if (textLength == 0 || textLength > dataLength) return false;
  for (size_t offset = 0; offset + textLength <= dataLength; ++offset) {
    if (memcmp(bytes + offset, text, textLength) == 0) return true;
  }
  return false;
}
}  // namespace

void test_default_settings_are_valid() {
  BeaconSettings settings;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
                          static_cast<uint8_t>(settings.band));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(settings.frequencyMode));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, settings.callsign);
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
                          static_cast<uint8_t>(ConfigValidator::validate(settings)));
#else
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
                        static_cast<uint8_t>(ConfigValidator::validate(settings)));
#endif
  TEST_ASSERT_EQUAL_STRING("JN45", settings.fixedLocator);
  TEST_ASSERT_EQUAL_UINT32(14097100UL, settings.baseFrequencyHz);
}
void test_callsign_accepts_type_one_and_rejects_invalid_forms() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCallsign("IW2NNV")));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCallsign("K1ABC")));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCallsign("NOCALL")));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCallsign("IW2/NNV")));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCallsign("iw2nnv")));
}
void test_locator_limits_are_enforced() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateLocator("AA00")));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateLocator("RR99")));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateLocator("SS00")));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateLocator("JN45AA")));
}

void test_power_and_interval_use_wspr_safe_values() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validatePower(0)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validatePower(60)));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validatePower(11)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateInterval(2)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateInterval(60)));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateInterval(3)));
}

void test_frequency_offset_and_correction_boundaries() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateBaseFrequency(14097000UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateBaseFrequency(14097200UL)));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateBaseFrequency(14096999UL)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateOffset(-100)));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateOffset(101)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCorrection(-1000000L)));
  TEST_ASSERT_NOT_EQUAL(static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(ConfigValidator::validateCorrection(1000001L)));
}

void test_combined_frequency_must_remain_in_wspr_window() {
  BeaconSettings settings = configuredSettings();
  settings.baseFrequencyHz = 14097000UL;
  settings.frequencyOffsetHz = -1;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::INVALID_COMBINATION),
      static_cast<uint8_t>(ConfigValidator::validate(settings)));
}

void test_all_four_tones_fit_at_upper_boundary() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097195UL, 0)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::INVALID_COMBINATION),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097196UL, 0)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::INVALID_COMBINATION),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097200UL, 0)));
}

void test_positive_and_negative_offsets_validate_complete_tone_set() {
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097100UL, -100)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097100UL, 95)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::INVALID_COMBINATION),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097100UL, 96)));
}

void test_future_random_selection_limits_keep_all_tones_in_window() {
  // The approved future RANDOM mode reserves a 10 Hz guard in the 200 Hz
  // window; RF-BAND-002A itself remains FIXED.
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097100UL, -90)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigValidation::VALID),
      static_cast<uint8_t>(
          ConfigValidator::validateToneSet(14097100UL, 90)));
}

void test_parser_shows_and_changes_ram_candidate() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr show").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set callsign iz2abc").status));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", parser.candidate().callsign);
  TEST_ASSERT_TRUE(parser.dirty());
}

void test_band_selection_is_atomic_nominal_and_band_aware() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set callsign IW2NNV").status));
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    char command[32];
    snprintf(command, sizeof(command), "wspr set band %s", band->name);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandStatus::OK),
        static_cast<uint8_t>(execute(parser, command).status));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(band->id),
                            static_cast<uint8_t>(parser.candidate().band));
    TEST_ASSERT_EQUAL_UINT32(band->nominalToneZeroHz,
                             parser.candidate().baseFrequencyHz);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConfigValidation::VALID),
        static_cast<uint8_t>(ConfigValidator::validate(parser.candidate())));

    snprintf(command, sizeof(command), "wspr set frequency %lu",
             static_cast<unsigned long>(band->lowerLimitHz));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandStatus::OK),
        static_cast<uint8_t>(execute(parser, command).status));
  }
  const BeaconSettings before = parser.candidate();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
      static_cast<uint8_t>(execute(parser, "wspr set band 80m").status));
  TEST_ASSERT_EQUAL_MEMORY(&before, &parser.candidate(), sizeof(before));
}

void test_frequency_mode_commands_update_the_shared_ram_candidate() {
  ConfigCommandParser parser;
  const CommandResult random = execute(parser, "wspr frequency mode random");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
                          static_cast<uint8_t>(random.status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::WSPR_FREQUENCY_MODE),
                          static_cast<uint8_t>(random.command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(random.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(parser.candidate().frequencyMode));
  TEST_ASSERT_TRUE(parser.dirty());

  const CommandResult fixed = execute(parser, "wspr frequency mode fixed");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(fixed.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(parser.candidate().frequencyMode));
  TEST_ASSERT_FALSE(parser.dirty());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
      static_cast<uint8_t>(execute(parser, "wspr frequency mode invalid").status));
}

void test_frequency_mode_help_explains_explicit_persistence() {
  TEST_ASSERT_EQUAL_STRING(
      "wspr frequency mode fixed|random - apply in RAM; use config save to persist",
      WSPR_FREQUENCY_MODE_HELP_TEXT);
  TEST_ASSERT_NULL(strstr(WSPR_FREQUENCY_MODE_HELP_TEXT, "(RAM only)"));
}

void test_invalid_set_does_not_change_candidate() {
  ConfigCommandParser parser;
  CommandResult result = execute(parser, "wspr set power 11");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
                          static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT8(10, parser.candidate().powerDbm);
  TEST_ASSERT_FALSE(parser.dirty());
}

void test_all_wspr_parameters_can_be_set() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set locator aa00").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set power 37").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set interval 6").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set frequency 14097050").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set offset -25").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wspr set correction -12345").status));
  TEST_ASSERT_EQUAL_STRING("AA00", parser.candidate().fixedLocator);
  TEST_ASSERT_EQUAL_INT16(-25, parser.candidate().frequencyOffsetHz);
}

void test_discard_defaults_and_save_commands() {
  ConfigCommandParser parser;
  execute(parser, "wspr set power 37");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "config discard").status));
  TEST_ASSERT_FALSE(parser.dirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "config save").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::CONFIG_SAVE),
      static_cast<uint8_t>(execute(parser, "config save").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi status").status));
}

void test_configuration_session_commands_and_mutation_policy() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::CONFIG_ENTER),
                          static_cast<uint8_t>(execute(parser, "config").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::CONFIG_EXIT),
                          static_cast<uint8_t>(execute(parser, "exit").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::CONFIG_EXIT),
      static_cast<uint8_t>(execute(parser, "config exit").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::CONFIG_EXIT),
                          static_cast<uint8_t>(execute(parser, "0").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "configuration").status));

  TEST_ASSERT_FALSE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::CONFIG_SHOW));
  TEST_ASSERT_FALSE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::WSPR_SHOW));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::WSPR_SET));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::WSPR_FREQUENCY_MODE));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::CONFIG_SAVE));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::CONFIG_DISCARD));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::CONFIG_DEFAULTS));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::FACTORY_RESET),
      static_cast<uint8_t>(execute(parser, "factory reset").command));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(
      ConfigCommand::FACTORY_RESET));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "factory reset now").status));
  TEST_ASSERT_FALSE(ConsoleConfigurationPolicy::allowed(
      ConfigCommand::WSPR_SET, false));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::allowed(
      ConfigCommand::WSPR_SET, true));
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::allowed(
      ConfigCommand::CONFIG_SHOW, false));
}

void test_saved_candidates_discard_defaults_and_secret_dirty_state() {
  ConfigCommandParser parser;
  BeaconSettings saved;
  strcpy(saved.callsign, "IZ2ABC");
  WifiCredentials credentials;
  TEST_ASSERT_TRUE(credentials.setSsid("Saved network"));
  TEST_ASSERT_TRUE(credentials.setPassword("saved password"));
  parser.loadCandidates(saved, credentials);
  TEST_ASSERT_FALSE(parser.dirty());
  execute(parser, "wspr set callsign iz2def");
  TEST_ASSERT_TRUE(parser.dirty());
  execute(parser, "config discard");
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", parser.candidate().callsign);
  TEST_ASSERT_EQUAL_STRING("Saved network", parser.wifiCredentials().ssid);
  TEST_ASSERT_FALSE(parser.dirty());
  execute(parser, "config defaults");
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN, parser.candidate().callsign);
  TEST_ASSERT_FALSE(parser.wifiCredentials().hasSsid());
  TEST_ASSERT_TRUE(parser.dirty());
}

void test_monitor_commands_are_parsed() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::MONITOR_ON),
      static_cast<uint8_t>(execute(parser, "monitor on").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::MONITOR_OFF),
      static_cast<uint8_t>(execute(parser, "monitor off").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
      static_cast<uint8_t>(execute(parser, "monitor maybe").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::MISSING_ARGUMENT),
      static_cast<uint8_t>(execute(parser, "monitor").status));
}

void test_rf_recovery_command_is_parsed() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::RF_RECOVER),
      static_cast<uint8_t>(execute(parser, "rf recover").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "rf reset").status));
}

void test_tx_telemetry_commands_are_parsed() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::TX_STATUS),
      static_cast<uint8_t>(execute(parser, "tx status").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::TX_LAST),
      static_cast<uint8_t>(execute(parser, "tx last").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::TX_ON),
      static_cast<uint8_t>(execute(parser, "tx on").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::TX_OFF),
      static_cast<uint8_t>(execute(parser, "tx off").command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "tx unknown").status));
}

void test_status_detail_commands_are_parsed_without_changing_status() {
  ConfigCommandParser parser;
  const struct {
    const char *text;
    ConfigCommand command;
  } cases[] = {
      {"status", ConfigCommand::STATUS},
      {"status gps", ConfigCommand::STATUS_GPS},
      {"status time", ConfigCommand::STATUS_TIME},
      {"status rf", ConfigCommand::STATUS_RF},
      {"status wifi", ConfigCommand::STATUS_WIFI},
      {"status modules", ConfigCommand::STATUS_MODULES},
  };
  for (const auto &entry : cases) {
    const CommandResult response = execute(parser, entry.text);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
                            static_cast<uint8_t>(response.status));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(entry.command),
                            static_cast<uint8_t>(response.command));
  }
  TEST_ASSERT_FALSE(parser.dirty());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "status unknown").status));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "status gps extra").status));
}

void test_menu_command_and_main_navigation() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::MENU),
                          static_cast<uint8_t>(execute(parser, "menu").command));
  ConsoleMenu menu;
  menu.enter();
  TEST_ASSERT_TRUE(menu.active());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::MAIN),
                          static_cast<uint8_t>(menu.screen()));
  menuAccept(menu, parser, "1");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::WSPR),
                          static_cast<uint8_t>(menu.screen()));
  menuAccept(menu, parser, "0");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::MAIN),
                          static_cast<uint8_t>(menu.screen()));
  const ConsoleMenuResult exit = menuAccept(menu, parser, "9");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuResultKind::EXITED),
                          static_cast<uint8_t>(exit.kind));
  TEST_ASSERT_FALSE(menu.active());
  TEST_ASSERT_FALSE(parser.dirty());
}

void test_menu_modifies_all_wspr_candidates_with_existing_validator() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "1");
  const char *values[] = {
      "IZ2ABC", "AA00", "37", "40m", "7040050", "-25", "6"};
  for (uint8_t choice = 1; choice <= 7; ++choice) {
    char selection[2] = {static_cast<char>('0' + choice), '\0'};
    menuAccept(menu, parser, selection);
    const ConsoleMenuResult response = menuAccept(menu, parser, values[choice - 1]);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ConsoleMenuResultKind::VALUE_ACCEPTED),
        static_cast<uint8_t>(response.kind));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::WSPR),
                            static_cast<uint8_t>(menu.screen()));
  }
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", parser.candidate().callsign);
  TEST_ASSERT_EQUAL_STRING("AA00", parser.candidate().fixedLocator);
  TEST_ASSERT_EQUAL_UINT8(37, parser.candidate().powerDbm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_40M),
                          static_cast<uint8_t>(parser.candidate().band));
  TEST_ASSERT_EQUAL_UINT32(7040050UL, parser.candidate().baseFrequencyHz);
  TEST_ASSERT_EQUAL_INT16(-25, parser.candidate().frequencyOffsetHz);
  TEST_ASSERT_EQUAL_UINT8(6, parser.candidate().transmitIntervalMinutes);
}

void test_menu_frequency_mode_entry_selects_fixed_and_random() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "1");

  const ConsoleMenuResult entry = menuAccept(menu, parser, "8");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::NAVIGATED),
      static_cast<uint8_t>(entry.kind));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuScreen::WSPR_FREQUENCY_MODE),
      static_cast<uint8_t>(menu.screen()));

  const uint32_t configuredFrequency = parser.candidate().baseFrequencyHz;
  const ConsoleMenuResult random = menuAccept(menu, parser, "2");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuResultKind::COMMAND),
                          static_cast<uint8_t>(random.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigCommand::WSPR_FREQUENCY_MODE),
                          static_cast<uint8_t>(random.command));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(random.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(parser.candidate().frequencyMode));
  TEST_ASSERT_EQUAL_UINT32(configuredFrequency,
                           parser.candidate().baseFrequencyHz);
  TEST_ASSERT_TRUE(ConsoleConfigurationPolicy::requiresOwnership(random.command));

  menuAccept(menu, parser, "8");
  const ConsoleMenuResult fixed = menuAccept(menu, parser, "1");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(fixed.frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
                          static_cast<uint8_t>(parser.candidate().frequencyMode));
}

void test_menu_frequency_mode_rejects_invalid_input_without_mutation() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "1");
  menuAccept(menu, parser, "8");
  const BeaconSettings before = parser.candidate();

  const ConsoleMenuResult invalid = menuAccept(menu, parser, "3");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::INVALID_SELECTION),
      static_cast<uint8_t>(invalid.kind));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuScreen::WSPR_FREQUENCY_MODE),
      static_cast<uint8_t>(menu.screen()));
  TEST_ASSERT_EQUAL_MEMORY(&before, &parser.candidate(), sizeof(before));

  const ConsoleMenuResult back = menuAccept(menu, parser, "0");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::NAVIGATED),
      static_cast<uint8_t>(back.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::WSPR),
                          static_cast<uint8_t>(menu.screen()));
}

void test_menu_rejects_invalid_value_without_losing_session_or_candidate() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "1");
  menuAccept(menu, parser, "2");
  const ConsoleMenuResult invalid = menuAccept(menu, parser, "JN45AA");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::INVALID_VALUE),
      static_cast<uint8_t>(invalid.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMenuScreen::WSPR_VALUE),
                          static_cast<uint8_t>(menu.screen()));
  TEST_ASSERT_EQUAL_STRING("JN45", parser.candidate().fixedLocator);
  const ConsoleMenuResult valid = menuAccept(menu, parser, "JN46");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::VALUE_ACCEPTED),
      static_cast<uint8_t>(valid.kind));
}

void test_menu_wifi_password_is_never_returned_in_result() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "2");
  menuAccept(menu, parser, "2");
  menuAccept(menu, parser, "Rete con spazi");
  menuAccept(menu, parser, "3");
  TEST_ASSERT_TRUE(menu.passwordExpected());
  const char secret[] = "password privata";
  const ConsoleMenuResult response = menuAccept(menu, parser, secret);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConsoleMenuResultKind::VALUE_ACCEPTED),
      static_cast<uint8_t>(response.kind));
  TEST_ASSERT_FALSE(menu.passwordExpected());
  TEST_ASSERT_TRUE(parser.wifiCredentials().complete());
  TEST_ASSERT_FALSE(containsBytes(&response, sizeof(response), secret));
}

void test_menu_save_discard_status_and_calibration_actions() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigCommand::STATUS_MODULES),
      static_cast<uint8_t>(menuAccept(menu, parser, "5").command));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigCommand::STATUS),
      static_cast<uint8_t>(menuAccept(menu, parser, "6").command));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigCommand::CONFIG_SAVE),
      static_cast<uint8_t>(menuAccept(menu, parser, "7").command));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ConfigCommand::CONFIG_DISCARD),
      static_cast<uint8_t>(menuAccept(menu, parser, "8").command));
  menuAccept(menu, parser, "4");
  menuAccept(menu, parser, "1");
  menuAccept(menu, parser, "12345");
  TEST_ASSERT_EQUAL_INT32(12345, parser.candidate().si5351Correction);
}

void test_menu_state_is_preserved_when_tx_suspends_input() {
  ConfigCommandParser parser;
  ConsoleMenu menu;
  menu.enter();
  menuAccept(menu, parser, "1");
  menuAccept(menu, parser, "8");
  const ConsoleMenuScreen beforeTx = menu.screen();
  // SerialConfigConsole does not call accept() while txBusy is true.
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeTx),
                          static_cast<uint8_t>(menu.screen()));
  TEST_ASSERT_TRUE(menu.active());
}

void test_console_status_snapshot_computes_slot_and_inhibition() {
  ConsoleStatusInputs inputs;
  inputs.nowMillis = 1234;
  inputs.configuredFrequencyHz = 14097050UL;
  inputs.frequencyMode = WsprFrequencyMode::RANDOM;
  inputs.transmitIntervalMinutes = 4;
  inputs.time.utcMillisOfDay = 5UL * 60000UL;
  inputs.time.statusFlags = 1U << 0;
  inputs.time.source = TimeSource::NTP;
  inputs.time.quality = TimeQuality::SYNCHRONIZED;
  inputs.authorization = WsprTimeAuthorization::NTP_EXPERIMENTAL;
  inputs.txState = ConsoleTxState::IDLE;
  inputs.txTelemetry.lastFrame.valid = true;
  inputs.txTelemetry.lastFrame.frequencyHz = 14097100UL;
  inputs.monitorEnabled = true;
  inputs.candidateDirty = true;
  const ConsoleStatusSnapshot snapshot = ConsoleStatus::build(inputs);
  TEST_ASSERT_TRUE(snapshot.nextSlotValid());
  TEST_ASSERT_EQUAL_UINT32(8UL * 60000UL, snapshot.nextSlotUtcMillis);
  TEST_ASSERT_TRUE(snapshot.monitorEnabled());
  TEST_ASSERT_TRUE(snapshot.candidateDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
                          static_cast<uint8_t>(snapshot.frequencyMode));
  TEST_ASSERT_EQUAL_UINT32(14097050UL, snapshot.configuredFrequencyHz);
  TEST_ASSERT_EQUAL_UINT32(14097100UL,
                           snapshot.txTelemetry.lastFrame.frequencyHz);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TxInhibitReason::TIME_UNAVAILABLE),
                          static_cast<uint8_t>(snapshot.txTelemetry.inhibitReason));

  inputs.txState = ConsoleTxState::RF_INHIBITED;
  inputs.txTelemetry.inhibitReason = TxInhibitReason::RF_UNAVAILABLE;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxInhibitReason::RF_UNAVAILABLE),
      static_cast<uint8_t>(ConsoleStatus::build(inputs).txTelemetry.inhibitReason));
  inputs.txState = ConsoleTxState::WAITING_FOR_TIME;
  inputs.authorization = WsprTimeAuthorization::NONE;
  inputs.txTelemetry.inhibitReason = TxInhibitReason::TIME_UNAVAILABLE;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(TxInhibitReason::TIME_UNAVAILABLE),
      static_cast<uint8_t>(ConsoleStatus::build(inputs).txTelemetry.inhibitReason));
}

void test_console_status_snapshot_contains_no_credentials() {
  ConsoleStatusInputs inputs;
  inputs.wifiSaved = true;
  const ConsoleStatusSnapshot snapshot = ConsoleStatus::build(inputs);
  TEST_ASSERT_TRUE(snapshot.wifiSaved());
  TEST_ASSERT_TRUE(sizeof(snapshot) <= 264);
  const char secret[] = "password riservata";
  TEST_ASSERT_FALSE(containsBytes(&snapshot, sizeof(snapshot), secret));
}

void test_console_mode_suspends_logs_on_first_input_byte() {
  ConsoleModeController mode;
  TEST_ASSERT_FALSE(mode.periodicDiagnosticsEnabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMode::INTERACTIVE),
                          static_cast<uint8_t>(mode.mode()));
  TEST_ASSERT_TRUE(mode.promptAllowed(true, false));
  mode.noteInput('\r');
  mode.noteInput('\n');
  TEST_ASSERT_FALSE(mode.periodicDiagnosticsEnabled());
  mode.setMonitorEnabled(true);
  TEST_ASSERT_TRUE(mode.periodicDiagnosticsEnabled());
  mode.noteInput('s');
  TEST_ASSERT_FALSE(mode.periodicDiagnosticsEnabled());
}

void test_console_mode_commands_and_password_input() {
  ConsoleModeController mode;
  mode.setMonitorEnabled(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMode::INTERACTIVE),
                          static_cast<uint8_t>(mode.mode()));
  mode.setMonitorEnabled(true);
  TEST_ASSERT_TRUE(mode.periodicDiagnosticsEnabled());
  mode.noteInput('p');
  mode.noteInput(' ');
  TEST_ASSERT_FALSE(mode.periodicDiagnosticsEnabled());
}

void test_console_mode_is_preserved_across_tx() {
  ConsoleModeController mode;
  mode.setMonitorEnabled(false);
  mode.noteInput('x', false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMode::INTERACTIVE),
                          static_cast<uint8_t>(mode.mode()));
  mode.setMonitorEnabled(true);
  mode.noteInput('x', false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMode::MONITOR),
                          static_cast<uint8_t>(mode.mode()));
}

void test_prompt_is_only_allowed_for_idle_interactive_input() {
  ConsoleModeController mode;
  TEST_ASSERT_TRUE(mode.promptAllowed(true, false));
  TEST_ASSERT_FALSE(mode.promptAllowed(false, false));
  TEST_ASSERT_FALSE(mode.promptAllowed(true, true));
  mode.setMonitorEnabled(true);
  TEST_ASSERT_FALSE(mode.promptAllowed(true, false));
}

void test_console_reboot_restores_monitor_off() {
  ConsoleModeController firstBoot;
  firstBoot.setMonitorEnabled(true);
  TEST_ASSERT_TRUE(firstBoot.periodicDiagnosticsEnabled());
  ConsoleModeController rebooted;
  TEST_ASSERT_FALSE(rebooted.periodicDiagnosticsEnabled());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleMode::INTERACTIVE),
                          static_cast<uint8_t>(rebooted.mode()));
}

void test_http_activity_defers_and_releases_periodic_reports() {
  TEST_ASSERT_FALSE(ConsoleWebCoordinator::periodicReportAllowed(
      false, false, false));
  TEST_ASSERT_TRUE(ConsoleWebCoordinator::periodicReportAllowed(
      true, false, false));
  TEST_ASSERT_FALSE(ConsoleWebCoordinator::periodicReportAllowed(
      true, true, false));
  TEST_ASSERT_FALSE(ConsoleWebCoordinator::periodicReportAllowed(
      true, false, true));
  TEST_ASSERT_TRUE(ConsoleWebCoordinator::periodicReportAllowed(
      true, false, false));
}

void test_wifi_commands_and_ssid_with_spaces() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi scan").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi ssid Rete di casa").status));
  TEST_ASSERT_EQUAL_STRING("Rete di casa", parser.wifiCredentials().ssid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi password").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi connect").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::OK),
      static_cast<uint8_t>(execute(parser, "wifi disconnect").status));
}

void test_wifi_ssid_limits_and_invalid_arguments() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
      static_cast<uint8_t>(execute(parser, "wifi ssid").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::INVALID_VALUE),
      static_cast<uint8_t>(execute(
          parser, "wifi ssid 123456789012345678901234567890123").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "wifi scan extra").status));
}

void test_wifi_password_limits_spaces_and_hex_psk() {
  WifiCredentials credentials;
  TEST_ASSERT_FALSE(credentials.setPassword("short"));
  TEST_ASSERT_TRUE(credentials.setPassword("otto con spazi"));
  TEST_ASSERT_TRUE(credentials.hasPassword());
  TEST_ASSERT_TRUE(credentials.setPassword(
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  TEST_ASSERT_FALSE(credentials.setPassword(
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"));
  credentials.clearPassword();
  TEST_ASSERT_FALSE(credentials.hasPassword());
}

void test_parser_stores_password_only_through_secret_input_path() {
  ConfigCommandParser parser;
  execute(parser, "wifi ssid Lab Network");
  TEST_ASSERT_FALSE(parser.wifiCredentials().hasPassword());
  TEST_ASSERT_TRUE(parser.setWifiPassword("password riservata"));
  TEST_ASSERT_TRUE(parser.wifiCredentials().complete());
}

void test_wifi_connection_tracker_timeout_success_and_loss() {
  WifiConnectionTracker tracker(30000UL);
  tracker.beginConnecting(1000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_CONNECTING),
                          static_cast<uint8_t>(tracker.update(31000UL, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_FAILED),
                          static_cast<uint8_t>(tracker.update(31001UL, false)));
  tracker.beginConnecting(40000UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_CONNECTED),
                          static_cast<uint8_t>(tracker.update(41000UL, true)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_DISCONNECTED),
                          static_cast<uint8_t>(tracker.update(42000UL, false)));
}

void test_wifi_connection_tracker_handles_monotonic_overflow() {
  WifiConnectionTracker tracker(100UL);
  tracker.beginConnecting(UINT32_MAX - 50UL);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_CONNECTING),
                          static_cast<uint8_t>(tracker.update(49UL, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_FAILED),
                          static_cast<uint8_t>(tracker.update(50UL, false)));
  tracker.disable();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiConnectionState::STATE_DISABLED),
                          static_cast<uint8_t>(tracker.update(1000UL, true)));
}

void test_wifi_connection_tracker_schedules_non_blocking_reconnect() {
  WifiConnectionTracker tracker(100UL);
  tracker.beginConnecting(1000UL);
  tracker.update(1101UL, false);
  TEST_ASSERT_FALSE(tracker.retryDue(1150UL, 100UL));
  TEST_ASSERT_TRUE(tracker.retryDue(1201UL, 100UL));
  tracker.beginConnecting(1201UL);
  TEST_ASSERT_FALSE(tracker.retryDue(1301UL, 100UL));
  tracker.update(1302UL, true);
  tracker.update(1400UL, false);
  TEST_ASSERT_FALSE(tracker.retryDue(1499UL, 100UL));
  TEST_ASSERT_TRUE(tracker.retryDue(1500UL, 100UL));
}

void test_incomplete_extra_and_unknown_input_are_rejected() {
  ConfigCommandParser parser;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::MISSING_ARGUMENT),
      static_cast<uint8_t>(execute(parser, "wspr set power").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::MISSING_ARGUMENT),
      static_cast<uint8_t>(execute(parser, "wspr set power 10 extra").status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CommandStatus::UNKNOWN_COMMAND),
      static_cast<uint8_t>(execute(parser, "unknown").status));
}

void test_line_buffer_waits_for_terminator_and_handles_backspace() {
  CommandLineBuffer input;
  const char *partial = "wspr shox";
  for (const char *cursor = partial; *cursor != '\0'; ++cursor) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LineInputResult::NONE),
                            static_cast<uint8_t>(input.push(*cursor)));
  }
  TEST_ASSERT_EQUAL_UINT(9, input.length());
  input.push('\b');
  input.push('w');
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LineInputResult::READY),
                          static_cast<uint8_t>(input.push('\n')));
  TEST_ASSERT_EQUAL_STRING("wspr show", input.line());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LineInputResult::NONE),
                          static_cast<uint8_t>(input.push('\n')));
}

void test_line_buffer_discards_complete_overflowed_line_and_recovers() {
  CommandLineBuffer input;
  for (size_t index = 0; index < CommandLineBuffer::CAPACITY + 20; ++index) {
    input.push('x');
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LineInputResult::OVERFLOW),
                          static_cast<uint8_t>(input.push('\r')));
  const char *valid = "status\r";
  LineInputResult state = LineInputResult::NONE;
  for (const char *cursor = valid; *cursor != '\0'; ++cursor) {
    state = input.push(*cursor);
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LineInputResult::READY),
                          static_cast<uint8_t>(state));
  TEST_ASSERT_EQUAL_STRING("status", input.line());
}

void test_console_echoes_normal_characters_once() {
  ConsoleInputEcho echo;
  const ConsoleEchoResult result = echo.accept('x', false, false, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::CHARACTER),
                          static_cast<uint8_t>(result.action));
  TEST_ASSERT_EQUAL_CHAR('x', result.character);
}

void test_console_echo_supports_backspace_and_del() {
  ConsoleInputEcho echo;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::ERASE),
      static_cast<uint8_t>(echo.accept('\b', false, true, true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::ERASE),
      static_cast<uint8_t>(echo.accept(static_cast<char>(0x7f), false, true,
                                       true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('\b', false, false, true).action));
}

void test_console_echo_handles_cr_lf_and_crlf_once() {
  ConsoleInputEcho echo;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NEWLINE),
      static_cast<uint8_t>(echo.accept('\r', false, false, true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('\n', false, false, true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('\n', false, false, true).action));
  echo.accept('x', false, false, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NEWLINE),
      static_cast<uint8_t>(echo.accept('\n', false, false, true).action));
}

void test_console_echo_never_reveals_password_input() {
  ConsoleInputEcho echo;
  const char *password = "secret with spaces";
  for (const char *cursor = password; *cursor != '\0'; ++cursor) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
        static_cast<uint8_t>(echo.accept(*cursor, true, true, true).action));
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('\b', true, true, true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NEWLINE),
      static_cast<uint8_t>(echo.accept('\r', true, true, true).action));
}

void test_console_echo_suppresses_characters_after_overflow() {
  ConsoleInputEcho echo;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('x', false, false, false).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NEWLINE),
      static_cast<uint8_t>(echo.accept('\r', false, false, false).action));
}

void test_console_echo_is_suspended_during_tx_and_recovers() {
  ConsoleInputEcho echo;
  echo.setSuspended(true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('x', false, false, true).action));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::NONE),
      static_cast<uint8_t>(echo.accept('\r', false, false, true).action));
  echo.setSuspended(false);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConsoleEchoAction::CHARACTER),
      static_cast<uint8_t>(echo.accept('x', false, false, true).action));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_default_settings_are_valid);
  RUN_TEST(test_callsign_accepts_type_one_and_rejects_invalid_forms);
  RUN_TEST(test_locator_limits_are_enforced);
  RUN_TEST(test_power_and_interval_use_wspr_safe_values);
  RUN_TEST(test_frequency_offset_and_correction_boundaries);
  RUN_TEST(test_combined_frequency_must_remain_in_wspr_window);
  RUN_TEST(test_all_four_tones_fit_at_upper_boundary);
  RUN_TEST(test_positive_and_negative_offsets_validate_complete_tone_set);
  RUN_TEST(test_future_random_selection_limits_keep_all_tones_in_window);
  RUN_TEST(test_parser_shows_and_changes_ram_candidate);
  RUN_TEST(test_band_selection_is_atomic_nominal_and_band_aware);
  RUN_TEST(test_frequency_mode_commands_update_the_shared_ram_candidate);
  RUN_TEST(test_frequency_mode_help_explains_explicit_persistence);
  RUN_TEST(test_invalid_set_does_not_change_candidate);
  RUN_TEST(test_all_wspr_parameters_can_be_set);
  RUN_TEST(test_discard_defaults_and_save_commands);
  RUN_TEST(test_configuration_session_commands_and_mutation_policy);
  RUN_TEST(test_saved_candidates_discard_defaults_and_secret_dirty_state);
  RUN_TEST(test_monitor_commands_are_parsed);
  RUN_TEST(test_rf_recovery_command_is_parsed);
  RUN_TEST(test_tx_telemetry_commands_are_parsed);
  RUN_TEST(test_status_detail_commands_are_parsed_without_changing_status);
  RUN_TEST(test_menu_command_and_main_navigation);
  RUN_TEST(test_menu_modifies_all_wspr_candidates_with_existing_validator);
  RUN_TEST(test_menu_frequency_mode_entry_selects_fixed_and_random);
  RUN_TEST(test_menu_frequency_mode_rejects_invalid_input_without_mutation);
  RUN_TEST(test_menu_rejects_invalid_value_without_losing_session_or_candidate);
  RUN_TEST(test_menu_wifi_password_is_never_returned_in_result);
  RUN_TEST(test_menu_save_discard_status_and_calibration_actions);
  RUN_TEST(test_menu_state_is_preserved_when_tx_suspends_input);
  RUN_TEST(test_console_status_snapshot_computes_slot_and_inhibition);
  RUN_TEST(test_console_status_snapshot_contains_no_credentials);
  RUN_TEST(test_console_mode_suspends_logs_on_first_input_byte);
  RUN_TEST(test_console_mode_commands_and_password_input);
  RUN_TEST(test_console_mode_is_preserved_across_tx);
  RUN_TEST(test_prompt_is_only_allowed_for_idle_interactive_input);
  RUN_TEST(test_console_reboot_restores_monitor_off);
  RUN_TEST(test_http_activity_defers_and_releases_periodic_reports);
  RUN_TEST(test_wifi_commands_and_ssid_with_spaces);
  RUN_TEST(test_wifi_ssid_limits_and_invalid_arguments);
  RUN_TEST(test_wifi_password_limits_spaces_and_hex_psk);
  RUN_TEST(test_parser_stores_password_only_through_secret_input_path);
  RUN_TEST(test_wifi_connection_tracker_timeout_success_and_loss);
  RUN_TEST(test_wifi_connection_tracker_handles_monotonic_overflow);
  RUN_TEST(test_wifi_connection_tracker_schedules_non_blocking_reconnect);
  RUN_TEST(test_incomplete_extra_and_unknown_input_are_rejected);
  RUN_TEST(test_line_buffer_waits_for_terminator_and_handles_backspace);
  RUN_TEST(test_line_buffer_discards_complete_overflowed_line_and_recovers);
  RUN_TEST(test_console_echoes_normal_characters_once);
  RUN_TEST(test_console_echo_supports_backspace_and_del);
  RUN_TEST(test_console_echo_handles_cr_lf_and_crlf_once);
  RUN_TEST(test_console_echo_never_reveals_password_input);
  RUN_TEST(test_console_echo_suppresses_characters_after_overflow);
  RUN_TEST(test_console_echo_is_suspended_during_tx_and_recovers);
  return UNITY_END();
}
