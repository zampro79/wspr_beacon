#include <unity.h>

#include <string.h>

#include "ConsoleStatusFormatter.h"

namespace {
struct OutputCapture {
  char text[4096] = {};
  size_t length = 0;
  size_t maxLineLength = 0;
};

void captureLine(void *context, const char *line) {
  OutputCapture &capture = *static_cast<OutputCapture *>(context);
  const size_t lineLength = strlen(line);
  if (lineLength > capture.maxLineLength) capture.maxLineLength = lineLength;
  if (capture.length + lineLength + 2 >= sizeof(capture.text)) return;
  memcpy(capture.text + capture.length, line, lineLength);
  capture.length += lineLength;
  capture.text[capture.length++] = '\n';
  capture.text[capture.length] = '\0';
}

bool contains(const OutputCapture &capture, const char *text) {
  return strstr(capture.text, text) != nullptr;
}

ConsoleStatusSnapshot populatedSnapshot() {
  ConsoleStatusSnapshot snapshot;
  snapshot.uptimeMillis = 60500UL;
  snapshot.configuredFrequencyHz = 14097100UL;
  snapshot.frequencyMode = WsprFrequencyMode::RANDOM;
  snapshot.nextSlotUtcMillis = 14UL * 3600000UL + 32UL * 60000UL;
  snapshot.time.utcMillisOfDay = 14UL * 3600000UL + 28UL * 60000UL + 59000UL;
  snapshot.time.referenceAgeMillis = 48932UL;
  snapshot.time.source = TimeSource::NTP;
  snapshot.time.quality = TimeQuality::SYNCHRONIZED;
  snapshot.time.statusFlags = 1U << 0;
  memcpy(snapshot.location.locator, "JN45", 5);
  snapshot.location.source = LocatorSource::FIXED;
  snapshot.authorization = WsprTimeAuthorization::NTP_EXPERIMENTAL;
  snapshot.wifiState = WifiConnectionState::STATE_CONNECTED;
  snapshot.txState = ConsoleTxState::IDLE;
  snapshot.txTelemetry.inhibitReason = TxInhibitReason::NONE;
  snapshot.txTelemetry.framesStarted = 7;
  snapshot.txTelemetry.framesCompleted = 6;
  snapshot.txTelemetry.framesInterrupted = 1;
  snapshot.txTelemetry.lastFrame.valid = true;
  snapshot.txTelemetry.lastFrame.frequencyHz = 14097189UL;
  snapshot.flags = (1U << 0) | (1U << 1) | (1U << 3);
  snapshot.http.flags =
      (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);
  snapshot.http.gateReason = HttpGateCloseReason::NONE;
  snapshot.http.lastGateTransitionMillis = UINT32_MAX - 43999UL;
  snapshot.http.gateOpenings = 2;
  snapshot.http.gateClosures = 1;
  snapshot.http.clientsAccepted = 4;
  snapshot.http.responsesServed = 3;
  snapshot.http.clientTimeouts = 1;
  snapshot.http.failedRequests = 2;
  snapshot.http.forcedClosures = 1;
  snapshot.http.bytesSent = 4096;
  snapshot.http.partialWrites = 2;
  snapshot.http.zeroWrites = 3;
  snapshot.http.noCapacityTurns = 4;
  snapshot.http.maxWriteMillis = 18;
  snapshot.http.maxResponseMillis = 1400;
  snapshot.http.lastResponseMillis = 700;
  snapshot.http.lastResponseBytes = 1250;
  snapshot.http.gateClientClosures = 1;
  snapshot.http.disconnectClosures = 2;
  snapshot.http.responsesInterrupted = 3;
  snapshot.http.flags |= (1U << 4) | (1U << 5);

  DiagnosticRecord &si5351 = snapshot.diagnostics.modules[2];
  si5351.state = DiagnosticState::STATE_DEGRADED;
  si5351.reason = DiagnosticReason::RF_OUTPUT_UNVERIFIED;
  si5351.lastChangeMillis = 600UL;
  si5351.counters.stateChanges = 2;
  si5351.counters.successes = 1;

  DiagnosticRecord &wifi = snapshot.diagnostics.modules[4];
  wifi.state = DiagnosticState::STATE_OPERATIONAL;
  wifi.reason = DiagnosticReason::NONE;
  wifi.lastChangeMillis = 1500UL;
  wifi.counters.stateChanges = 3;
  wifi.counters.successes = 4;

  DiagnosticRecord &ntp = snapshot.diagnostics.modules[5];
  ntp.state = DiagnosticState::STATE_OPERATIONAL;
  ntp.reason = DiagnosticReason::NONE;
  ntp.lastChangeMillis = 1600UL;
  ntp.counters.stateChanges = 5;
  ntp.counters.successes = 6;
  return snapshot;
}
}  // namespace

void test_summary_has_aligned_sections_and_all_existing_information() {
  OutputCapture output;
  const ConsoleStatusSnapshot snapshot = populatedSnapshot();
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP8266", captureLine,
                                       &output);

  TEST_ASSERT_TRUE(contains(output, "WSPR BEACON 2.0 | ESP8266"));
  TEST_ASSERT_TRUE(contains(output, " [TIME]\n"));
  TEST_ASSERT_TRUE(contains(output, " [LOCATION]\n"));
  TEST_ASSERT_TRUE(contains(output, " [RADIO]\n"));
  TEST_ASSERT_TRUE(contains(output, " [HARDWARE]\n"));
  TEST_ASSERT_TRUE(contains(output, " [NETWORK]\n"));
  TEST_ASSERT_TRUE(contains(output, " [HTTP]\n"));
  TEST_ASSERT_TRUE(contains(output, " UTC          : 14:28:59"));
  TEST_ASSERT_TRUE(contains(output, " Uptime       : 00:01:00.500"));
  TEST_ASSERT_TRUE(contains(output, " Source       : NTP"));
  TEST_ASSERT_TRUE(contains(output, " Quality      : SYNCHRONIZED"));
  TEST_ASSERT_TRUE(contains(output, " TX Authority : NTP_EXPERIMENTAL"));
  TEST_ASSERT_TRUE(contains(output, " Sync Age     : 48.932 s"));
  TEST_ASSERT_TRUE(contains(output, " Locator      : JN45"));
  TEST_ASSERT_TRUE(contains(output, " Source       : FIXED"));
  TEST_ASSERT_TRUE(contains(output, " State        : IDLE"));
  TEST_ASSERT_TRUE(contains(output, " Mode         : RANDOM"));
  TEST_ASSERT_TRUE(contains(output, " Config Freq  : 14.097100 MHz"));
  TEST_ASSERT_TRUE(contains(output, " Last TX Freq : 14.097189 MHz"));
  TEST_ASSERT_TRUE(contains(output, " Next Slot    : 14:32:00 UTC"));
  TEST_ASSERT_TRUE(contains(output, " RF Inhibit   : NONE"));
  TEST_ASSERT_TRUE(contains(output,
                            " Frames       : 7 started / 6 completed / 1 aborted"));
  TEST_ASSERT_TRUE(contains(output, " Si5351A     : DEGRADED (RF_UNVERIFIED)"));
  TEST_ASSERT_TRUE(contains(output, " Age         : 59.900 s"));
  TEST_ASSERT_TRUE(contains(output, " Counters    : 2 changes / 1 ok / 0 errors"));
  TEST_ASSERT_TRUE(contains(output, " WiFi         : CONNECTED (SAVED)"));
  TEST_ASSERT_TRUE(contains(output, " WiFi Diag   : OPERATIONAL (NONE)"));
  TEST_ASSERT_TRUE(contains(output, " NTP         : OPERATIONAL (NONE)"));
  TEST_ASSERT_TRUE(contains(output, " Gate         : OPEN (NONE)"));
  TEST_ASSERT_TRUE(contains(output, " Gate Age     : 104.500 s"));
  TEST_ASSERT_TRUE(contains(output, " Gate Count   : 2 open / 1 closed"));
  TEST_ASSERT_TRUE(contains(output, " Listener     : intended ON"));
  TEST_ASSERT_TRUE(contains(output, " Clients      : 4 accepted"));
  TEST_ASSERT_TRUE(contains(
      output, " Responses    : 3 served / 1 timeout (0 req / 0 resp)"));
  TEST_ASSERT_TRUE(contains(output,
                            " Failures     : 2 request / 1 forced close"));
  TEST_ASSERT_TRUE(contains(output,
                            " Transfer     : 4096 bytes / 2 partial / 3 zero"));
  TEST_ASSERT_TRUE(contains(output,
                            " TCP capacity : 4 zero-capacity turns"));
  TEST_ASSERT_TRUE(contains(output,
                            " Latency max  : write 18 ms / response 1400 ms"));
  TEST_ASSERT_TRUE(contains(output,
                            " Last response: 700 ms / 1250 bytes / complete"));
  TEST_ASSERT_TRUE(contains(output,
                            " Closures     : 1 gate / 2 disconnect / 3 interrupted"));
  TEST_ASSERT_TRUE(output.maxLineLength <= 80);
}

void test_fixed_mode_and_missing_values_are_explicit() {
  OutputCapture output;
  ConsoleStatusSnapshot snapshot;
  snapshot.uptimeMillis = 3UL * 86400000UL + 3723004UL;
  snapshot.configuredFrequencyHz = 14097010UL;
  snapshot.frequencyMode = WsprFrequencyMode::FIXED;
  memcpy(snapshot.location.locator, "----", 5);
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP32", captureLine, &output);

  TEST_ASSERT_TRUE(contains(output, " UTC          : --"));
  TEST_ASSERT_TRUE(contains(output, " Uptime       : 3d 01:02:03.004"));
  TEST_ASSERT_TRUE(contains(output, " Mode         : FIXED"));
  TEST_ASSERT_TRUE(contains(output, " Config Freq  : 14.097010 MHz"));
  TEST_ASSERT_TRUE(contains(output, " Last TX Freq : --"));
  TEST_ASSERT_TRUE(contains(output, " Next Slot    : --"));
  TEST_ASSERT_TRUE(contains(output, " Sync Age     : --"));
  TEST_ASSERT_TRUE(contains(output, " WiFi         : DISABLED (NOT_SAVED)"));
}

void test_http_gate_age_is_unavailable_for_invalid_or_future_transition() {
  OutputCapture output;
  ConsoleStatusSnapshot snapshot = populatedSnapshot();
  snapshot.http.flags &= ~(1U << 3);
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP8266", captureLine,
                                       &output);
  TEST_ASSERT_TRUE(contains(output, " Gate Age     : --"));

  OutputCapture futureOutput;
  snapshot.http.flags |= 1U << 3;
  snapshot.uptimeMillis = 100;
  snapshot.http.lastGateTransitionMillis = 101;
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP8266", captureLine,
                                       &futureOutput);
  TEST_ASSERT_TRUE(contains(futureOutput, " Gate Age     : --"));
}

void test_monitor_output_is_forbidden_during_preparing_and_tx() {
  TEST_ASSERT_TRUE(ConsoleStatusFormatter::outputAllowed(ConsoleTxState::IDLE));
  TEST_ASSERT_TRUE(
      ConsoleStatusFormatter::outputAllowed(ConsoleTxState::WAITING_FOR_TIME));
  TEST_ASSERT_FALSE(
      ConsoleStatusFormatter::outputAllowed(ConsoleTxState::PREPARING));
  TEST_ASSERT_FALSE(
      ConsoleStatusFormatter::outputAllowed(ConsoleTxState::TRANSMITTING));
}

void test_operating_mode_owners_and_resume_guard_are_compact() {
  OutputCapture output;
  ConsoleStatusSnapshot snapshot = populatedSnapshot();
  snapshot.operatingMode.mode = FirmwareOperatingMode::CONFIGURATION;
  snapshot.operatingMode.cliActive = true;
  snapshot.operatingMode.webActive = true;
  snapshot.operatingMode.revision = 9;
  snapshot.operatingMode.txAdmissionAllowed = false;
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP8266", captureLine,
                                       &output);
  TEST_ASSERT_TRUE(contains(output, " Operating    : CONFIGURATION"));
  TEST_ASSERT_TRUE(contains(output, " Sessions     : CLI+WEB"));
  TEST_ASSERT_TRUE(contains(output, " TX Admission : INHIBITED (rev 9)"));

  OutputCapture guardOutput;
  snapshot.operatingMode = ConfigurationSessionSnapshot();
  snapshot.operatingMode.resumeGuardActive = true;
  snapshot.operatingMode.txAdmissionAllowed = false;
  ConsoleStatusFormatter::writeSummary(snapshot, "ESP8266", captureLine,
                                       &guardOutput);
  TEST_ASSERT_TRUE(contains(guardOutput,
                            " TX Admission : RESUME_GUARD (rev 0)"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_summary_has_aligned_sections_and_all_existing_information);
  RUN_TEST(test_fixed_mode_and_missing_values_are_explicit);
  RUN_TEST(test_http_gate_age_is_unavailable_for_invalid_or_future_transition);
  RUN_TEST(test_monitor_output_is_forbidden_during_preparing_and_tx);
  RUN_TEST(test_operating_mode_owners_and_resume_guard_are_compact);
  return UNITY_END();
}
