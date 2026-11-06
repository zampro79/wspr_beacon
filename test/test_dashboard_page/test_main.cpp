#include <unity.h>

#include <string>

#include "DashboardPage.h"

namespace {
std::string page() {
  return std::string(dashboardPageData(), dashboardPageSize());
}

void contains(const std::string &text, const char *needle) {
  TEST_ASSERT_NOT_EQUAL(std::string::npos, text.find(needle));
}

void excludes(const std::string &text, const char *needle) {
  TEST_ASSERT_EQUAL(std::string::npos, text.find(needle));
}

struct PollRecoveryModel {
  static constexpr uint32_t FAST_MS = 15000U;
  static constexpr uint32_t SLOW_MS = 30000U;
  static constexpr uint8_t MAX_FAST = 3U;

  bool timingUnknown = false;
  bool pollInFlight = false;
  uint8_t recoveryAttempts = 0;
  uint8_t pollFailures = 0;

  uint32_t nextDelay(bool knownWindow) const {
    if (knownWindow) return FAST_MS;
    if (!timingUnknown) return pollFailures >= 3U ? FAST_MS : 3000U;
    if (recoveryAttempts == 0U) return 1000U;
    return recoveryAttempts < MAX_FAST ? FAST_MS : SLOW_MS;
  }

  bool startAttempt(bool knownWindow) {
    if (pollInFlight || knownWindow) return false;
    pollInFlight = true;
    if (timingUnknown && recoveryAttempts < UINT8_MAX) ++recoveryAttempts;
    return true;
  }

  void failAttempt() {
    pollInFlight = false;
    if (pollFailures < 3U) ++pollFailures;
    if (pollFailures >= 3U) timingUnknown = true;
  }

  void acceptStatus() {
    pollInFlight = false;
    timingUnknown = false;
    recoveryAttempts = 0;
    pollFailures = 0;
  }
};

struct NavigationPriorityModel {
  bool navigationPending = false;
  bool pollInFlight = false;
  bool pollTimerArmed = true;
  bool waitingForPoll = false;
  uint8_t pollFailures = 0;
  uint8_t navigations = 0;

  bool click(bool blocked) {
    if (blocked || navigationPending) return false;
    navigationPending = true;
    pollTimerArmed = false;
    waitingForPoll = pollInFlight;
    return true;
  }

  bool finishPoll(bool succeeded, bool blockedAfterWait = false) {
    pollInFlight = false;
    waitingForPoll = false;
    if (!succeeded && pollFailures < UINT8_MAX) ++pollFailures;
    if (!succeeded || blockedAfterWait) {
      finishNavigation(false);
      return false;
    }
    return true;
  }

  void finishNavigation(bool started) {
    if (started) {
      ++navigations;
      return;
    }
    navigationPending = false;
    pollTimerArmed = true;
  }
};
}  // namespace

void test_asset_is_complete_and_bounded() {
  const std::string html = page();
  TEST_ASSERT_EQUAL_UINT32(html.size(), dashboardPageSize());
  TEST_ASSERT_TRUE(html.size() < 26624U);
  TEST_ASSERT_EQUAL_STRING("<!doctype html>", html.substr(0, 15).c_str());
  contains(html, "</html>");
}

void test_page_is_mobile_first_and_theme_aware() {
  const std::string html = page();
  contains(html, "width=device-width,initial-scale=1");
  contains(html, "minmax(min(100%,18rem),1fr)");
  contains(html, "@media(max-width:420px)");
  contains(html, "prefers-color-scheme:dark");
  contains(html, "overflow-x:hidden");
  contains(html, "h1{font-size:1.35rem;margin:0;overflow-wrap:anywhere}");
}

void test_header_and_navigation_match_configuration_page() {
  const std::string html = page();
  contains(html, "<h1>WSPR Beacon 2.0</h1>");
  contains(html, "class=\"credit\">Developed by IW2NNV / AC2WZ");
  contains(html, "<nav class=\"nav\" aria-label=\"Primary\">");
  contains(html, "<button type=\"button\" class=\"current\" aria-current=\"page\">Dashboard</button>");
  excludes(html, "href=\"/\" aria-current=\"page\"");
  contains(html, "href=\"/config\" id=\"configurationLink\">Configuration</a>");
  contains(html, ".nav a{min-height:2.75rem");
  contains(html, ".nav button.current{min-height:2.75rem");
  excludes(html, "id=\"identity\"");
  excludes(html, "[i.callsign,i.locator,i.band]");
  excludes(html, "IW2NNV · JN45 · 20m");
  contains(html, "id=\"callsign\"");
  contains(html, "id=\"locator\"");
  contains(html, "put('callsign',i.callsign)");
  contains(html, "id==='callsign'?'Not configured':'--'");
  contains(html, "put('locator',i.locator)");
  excludes(html, "Cerca reti");
  excludes(html, "Utilizza senza rete Wi-Fi");
}

void test_page_contains_required_read_only_sections() {
  const std::string html = page();
  contains(html, "Next transmission");
  contains(html, "Last transmission");
  contains(html, "Time synchronization");
  contains(html, "RF status");
  contains(html, "Configured frequency");
  contains(html, "Last frame");
  contains(html, "Last frequency");
  contains(html, "Declared power");
  contains(html, "GPS receiver");
  contains(html, "GPS details");
  contains(html, "Locator source");
  contains(html, "Reference age");
  contains(html, "<details>");
  contains(html, "Diagnostics");
  excludes(html, "<form");
  contains(html, "Manual TX control");
  contains(html, "Manual TX permission");
  contains(html, "Effective TX eligibility");
  contains(html, "id=\"txOff\"");
  contains(html, "id=\"txOn\"");
  contains(html, "id=\"manualPermission\">TX ON");
  contains(html, "TX ON is the boot default");
  contains(html, "TX OFF is temporary until TX ON or restart");
  contains(html, "Manual TX permission never overrides safety gates.");
  contains(html, "X-WSPR-CSRF-Token");
  contains(html, "/api/tx/v1/");
  excludes(html, "config save");
}

void test_page_has_no_external_dependencies() {
  const std::string html = page();
  excludes(html, "https://");
  excludes(html, "http://");
  excludes(html, "<script src=");
  excludes(html, "<link rel=");
}

void test_polling_is_bounded_serial_and_schema_checked() {
  const std::string html = page();
  contains(html, "POLL_MS=3000");
  contains(html, "TIMEOUT_MS=1800");
  contains(html, "AbortController");
  contains(html, "setTimeout(poll,nextPollDelay(now))");
  contains(html, "data.schema!==1");
  contains(html, "fetch('/api/status'");
  TEST_ASSERT_EQUAL_UINT32(html.find("fetch('/api/status'"),
                           html.rfind("fetch('/api/status'"));
}

void test_stale_and_unavailable_states_are_explicit() {
  const std::string html = page();
  contains(html, "STALE_MS=6000");
  contains(html, "UNAVAILABLE_MS=30000");
  contains(html, "fresh?'LIVE':available?'STALE':'UNAVAILABLE'");
  contains(html, "'STALE':'UNAVAILABLE'");
  contains(html, "Data unavailable; last values retained");
  contains(html, "'LAST · '+last.state");
  contains(html, "age>=0&&age<=STALE_MS");
  contains(html, "age>=0&&age<=UNAVAILABLE_MS");
}

void test_null_fixed_and_random_frequency_semantics_are_present() {
  const std::string html = page();
  contains(html, "value==null||value===''?");
  contains(html, "i.frequency_mode==='RANDOM'");
  contains(html, "t.configured_frequency_hz");
  contains(html, "random?'RANDOM'");
  contains(html, "Next frequency selected at PREPARING");
  contains(html, "lastTx.valid?mhz(lastTx.frequency_hz):'--'");
  excludes(html, "Selected frame");
  excludes(html, "id=\"selectedFreq\"");
}

void test_last_frame_message_uses_authoritative_last_telemetry() {
  const std::string html = page();
  contains(html, "lastTx.valid&&present(lastTx.callsign)");
  contains(html, "lastTx.callsign+' '+lastTx.locator+' '+lastTx.power_dbm");
  contains(html, "put('lastFrame',lastMessage)");
  excludes(html, "put('lastFrame',[i.callsign");
  excludes(html, "lastTx.locator+' '+i.power_dbm");
}

void test_gps_card_uses_existing_schema_fields_conservatively() {
  const std::string html = page();
  contains(html, "id=\"gpsReceiver\"");
  contains(html, "id=\"gpsFix\"");
  contains(html, "id=\"gpsSatellites\"");
  contains(html, "id=\"gpsNmea\"");
  contains(html, "id=\"gpsLatitude\"");
  contains(html, "id=\"gpsLongitude\"");
  contains(html, "id=\"gpsPpsPresence\"");
  contains(html, "id=\"gpsPpsAssociation\"");
  contains(html, "id=\"gpsLocatorSource\"");
  contains(html, "id=\"gpsDiagnostic\"");
  contains(html, "fix&&number(g.latitude)?g.latitude.toFixed(6):'--'");
  contains(html, "fix&&number(g.longitude)?g.longitude.toFixed(6):'--'");
  contains(html, "hasNmea&&present(g.satellites)?g.satellites:'--'");
  contains(html, "safe(modules.gps)");
  contains(html, "i.locator_origin");
}

void test_last_tx_result_has_a_human_readable_label() {
  const std::string html = page();
  contains(html, "v==='COMPLETED_BY_FIRMWARE'?'Completed':v");
  contains(html, "Firmware completion does not confirm reception");
}

void test_estimate_uses_authoritative_eligibility_and_monotonic_time() {
  const std::string html = page();
  contains(html, "performance.now()");
  contains(html, "t.state==='IDLE'");
  contains(html, "t.rf_ready===true");
  contains(html, "t.inhibit_reason==='NONE'");
  contains(html, "tm.tx_authority!=='NONE'");
  contains(html, "number(t.next_slot_utc_ms_of_day)");
  contains(html, "number(t.remaining_ms)");
  excludes(html, "next_slot_utc_ms_of_day+");
}

void test_estimated_phases_duration_and_progress_are_bounded() {
  const std::string html = page();
  contains(html, "FRAME_MS=110600");
  contains(html, "until>5000");
  contains(html, "RF window approaching");
  contains(html, "until>1000");
  contains(html, "Preparing expected");
  contains(html, "TX expected");
  contains(html, "duration(FRAME_MS+until)");
  contains(html, "Math.min(100,Math.max(0,-until/FRAME_MS*100))");
  contains(html, "Awaiting telemetry");
  contains(html, "completion is not inferred");
}

void test_estimate_handles_missing_data_and_timer_discontinuity() {
  const std::string html = page();
  contains(html, "estimate=eligible?");
  contains(html, ":null");
  contains(html, "DISCONTINUITY_MS=10000");
  contains(html, "step<0||step>DISCONTINUITY_MS");
  contains(html, "elapsed<0||elapsed>estimate.remaining+FRAME_MS+60000");
  contains(html, "Timer discontinuity; no TX state inferred");
  contains(html, "armEstimate(t,tm,op,now,started)");
}

void test_api_values_are_written_as_text_not_markup() {
  const std::string html = page();
  contains(html, ".textContent=");
  excludes(html, "innerHTML");
  excludes(html, "document.write");
  excludes(html, "eval(");
}

void test_all_schema_sections_have_consumers() {
  const std::string html = page();
  contains(html, "data.identity");
  contains(html, "data.tx");
  contains(html, "data.time");
  contains(html, "data.network");
  contains(html, "data.gps");
  contains(html, "data.modules");
  contains(html, "data.system");
  contains(html, "data.operating");
}

void test_configuration_and_resume_guard_suppress_tx_estimate() {
  const std::string html = page();
  contains(html, "id=\"operatingMode\"");
  contains(html, "CONFIGURATION — TX DISABLED");
  contains(html, "TX RESUME GUARD");
  contains(html, "op.mode!=='CONFIGURATION'");
  contains(html, "op.resume_guard!==true");
  contains(html, "op.tx_admission_allowed===true");
  contains(html, "Configuration session active; no transmission countdown");
  contains(html, "Waiting for a safe future scheduler slot");
}

void test_tx_window_pauses_requests_and_labels_estimates() {
  const std::string html = page();
  contains(html, "WSPR transmission window");
  contains(html, "Web updates are paused to protect WSPR timing.");
  contains(html, "Controls are temporarily unavailable.");
  contains(html, "Estimated — not live");
  contains(html, "PAUSE_LEAD_MS=12000");
  contains(html, "PAUSE_TAIL_MS=3000");
  contains(html, "txWindow=gateLikely?");
  contains(html, "if(windowPaused(started)||txWindow&&started<txWindow.start");
  contains(html, "schedulePoll(started);return");
  contains(html, "pollFailures>=3?RECOVERY_MS:POLL_MS");
  contains(html, "SLOW_RECOVERY_MS=30000");
  contains(html, "document.visibilityState==='hidden'");
  contains(html, "now-lastOk<=STALE_MS");
  contains(html, "el('txOn').disabled=navigationPending||paused||timingUnknown||!fresh");
  contains(html, "el('txOff').disabled=navigationPending||paused||timingUnknown||!fresh");
  contains(html, "'LAST · '+last.state");
  excludes(html, "phase='TRANSMITTING'");
}

void test_timing_uncertainty_and_bounded_recovery_are_explicit() {
  const std::string html = page();
  contains(html, "Timing estimate unavailable — waiting for fresh status");
  contains(html, "const started=performance.now()");
  contains(html, "const received=performance.now()");
  contains(html, "received-started>MAX_RESPONSE_AGE_MS");
  contains(html, "start:started+t.remaining_ms-PAUSE_LEAD_MS");
  contains(html, "end:received+t.remaining_ms+FRAME_MS+PAUSE_TAIL_MS");
  contains(html, "Math.abs(started+t.remaining_ms-txWindow.deadline)>REFERENCE_SHIFT_MS");
  contains(html, "markTimingUnknown(now,RECOVERY_MS)");
  contains(html, "markTimingUnknown(received,FRAME_MS+PAUSE_LEAD_MS+PAUSE_TAIL_MS)");
  contains(html, "markTimingUnknown(performance.now(),FRAME_MS+PAUSE_LEAD_MS+PAUSE_TAIL_MS)");
  contains(html, "recoveryAttempts<MAX_RECOVERY_ATTEMPTS?RECOVERY_MS:SLOW_RECOVERY_MS");
  excludes(html, "recoveryAttempts>=MAX_RECOVERY_ATTEMPTS)return null");
  contains(html, "now<recoveryAt");
  contains(html, "activeRequest.abort()");
  contains(html, "now-hiddenAt>DISCONTINUITY_MS");
  contains(html, "function restartTimingRecovery(now=performance.now())");
  contains(html, "restartTimingRecovery(now);if(activeRequest)activeRequest.abort()");
  contains(html, "if(windowPaused(started)");
  excludes(html, "Date.now()");
}

void test_internal_navigation_waits_for_authoritative_service() {
  const std::string html = page();
  contains(html, "href=\"/config\" id=\"configurationLink\"");
  contains(html, "id=\"navigationNotice\" role=\"status\" hidden");
  contains(html, "Navigation temporarily unavailable");
  contains(html, "function navigationBlocked(now=performance.now())");
  contains(html, "!fresh||timingUnknown||windowPaused(now)");
  contains(html, "last.state==='PREPARING'||last.state==='TRANSMITTING'");
  contains(html, "async function navigateToConfiguration(event)");
  contains(html, "if(navigationBlocked()){event.preventDefault();navigationNotice();return}");
  contains(html, "const pendingPoll=pollInFlight?pollSettled:Promise.resolve(true)");
  contains(html, "navigationPending=true;updatePause();clearTimeout(pollTimer)");
  contains(html, "if(!await pendingPoll)throw Error('Status poll failed')");
  contains(html, "refreshTiming();if(navigationBlocked())throw Error('Navigation no longer safe')");
  contains(html, "done(succeeded)");
  contains(html, "clearTimeout(pauseAbortTimer);location.assign('/config')");
  excludes(html, "clearTimeout(pauseAbortTimer);if(activeRequest)activeRequest.abort()");
  contains(html, "if(!navigationPending)pollTimer=setTimeout(poll,nextPollDelay(now))");
  contains(html, "if(!navigationBlocked(now))el('navigationNotice').hidden=true");
  contains(html, "addEventListener('click',navigateToConfiguration)");
  excludes(html, "fetch('/config'");
}

void test_current_page_enter_activation_is_request_free() {
  const std::string html = page();
  contains(html, "<button type=\"button\" class=\"current\" aria-current=\"page\">Dashboard</button>");
  excludes(html, "href=\"/\" aria-current=\"page\"");
  excludes(html, "location.reload");
  excludes(html, "location.href='/'");
  excludes(html, "fetch('/'");
}

void test_recovery_policy_model_keeps_slow_retries_after_fast_limit() {
  PollRecoveryModel model;
  model.timingUnknown = true;
  for (uint8_t attempt = 0; attempt < 3U; ++attempt) {
    TEST_ASSERT_TRUE(model.startAttempt(false));
    model.failAttempt();
  }
  TEST_ASSERT_EQUAL_UINT32(PollRecoveryModel::SLOW_MS,
                           model.nextDelay(false));
  for (uint8_t attempt = 0; attempt < 5U; ++attempt) {
    TEST_ASSERT_TRUE(model.startAttempt(false));
    model.failAttempt();
    TEST_ASSERT_EQUAL_UINT32(PollRecoveryModel::SLOW_MS,
                             model.nextDelay(false));
  }
}

void test_recovery_policy_model_valid_status_resets_every_counter() {
  PollRecoveryModel model;
  model.timingUnknown = true;
  model.recoveryAttempts = 9U;
  model.pollFailures = 3U;
  TEST_ASSERT_TRUE(model.startAttempt(false));
  model.acceptStatus();
  TEST_ASSERT_FALSE(model.timingUnknown);
  TEST_ASSERT_FALSE(model.pollInFlight);
  TEST_ASSERT_EQUAL_UINT8(0U, model.recoveryAttempts);
  TEST_ASSERT_EQUAL_UINT8(0U, model.pollFailures);
  TEST_ASSERT_EQUAL_UINT32(3000U, model.nextDelay(false));
}

void test_recovery_policy_model_avoids_known_window_and_overlap() {
  PollRecoveryModel model;
  model.timingUnknown = true;
  TEST_ASSERT_FALSE(model.startAttempt(true));
  TEST_ASSERT_EQUAL_UINT8(0U, model.recoveryAttempts);
  TEST_ASSERT_TRUE(model.startAttempt(false));
  TEST_ASSERT_FALSE(model.startAttempt(false));
  model.failAttempt();
  TEST_ASSERT_TRUE(model.startAttempt(false));
}

void test_navigation_priority_model_handles_idle_active_and_repeated_clicks() {
  NavigationPriorityModel idle;
  TEST_ASSERT_TRUE(idle.click(false));
  TEST_ASSERT_FALSE(idle.pollTimerArmed);
  TEST_ASSERT_FALSE(idle.waitingForPoll);
  TEST_ASSERT_FALSE(idle.click(false));
  idle.finishNavigation(true);
  TEST_ASSERT_EQUAL_UINT8(1U, idle.navigations);

  NavigationPriorityModel active;
  active.pollInFlight = true;
  TEST_ASSERT_TRUE(active.click(false));
  TEST_ASSERT_TRUE(active.waitingForPoll);
  TEST_ASSERT_TRUE(active.finishPoll(true));
  TEST_ASSERT_EQUAL_UINT8(0U, active.pollFailures);
  TEST_ASSERT_FALSE(active.pollTimerArmed);
}

void test_navigation_priority_model_recovers_and_preserves_blocks() {
  NavigationPriorityModel blocked;
  TEST_ASSERT_FALSE(blocked.click(true));
  TEST_ASSERT_FALSE(blocked.navigationPending);
  TEST_ASSERT_TRUE(blocked.pollTimerArmed);

  NavigationPriorityModel failed;
  failed.pollInFlight = true;
  TEST_ASSERT_TRUE(failed.click(false));
  TEST_ASSERT_FALSE(failed.finishPoll(false));
  TEST_ASSERT_EQUAL_UINT8(1U, failed.pollFailures);
  TEST_ASSERT_FALSE(failed.navigationPending);
  TEST_ASSERT_TRUE(failed.pollTimerArmed);
  TEST_ASSERT_TRUE(failed.click(false));

  NavigationPriorityModel becameCritical;
  becameCritical.pollInFlight = true;
  TEST_ASSERT_TRUE(becameCritical.click(false));
  TEST_ASSERT_FALSE(becameCritical.finishPoll(true, true));
  TEST_ASSERT_EQUAL_UINT8(0U, becameCritical.navigations);
  TEST_ASSERT_TRUE(becameCritical.pollTimerArmed);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_asset_is_complete_and_bounded);
  RUN_TEST(test_page_is_mobile_first_and_theme_aware);
  RUN_TEST(test_header_and_navigation_match_configuration_page);
  RUN_TEST(test_page_contains_required_read_only_sections);
  RUN_TEST(test_page_has_no_external_dependencies);
  RUN_TEST(test_polling_is_bounded_serial_and_schema_checked);
  RUN_TEST(test_stale_and_unavailable_states_are_explicit);
  RUN_TEST(test_null_fixed_and_random_frequency_semantics_are_present);
  RUN_TEST(test_last_frame_message_uses_authoritative_last_telemetry);
  RUN_TEST(test_gps_card_uses_existing_schema_fields_conservatively);
  RUN_TEST(test_last_tx_result_has_a_human_readable_label);
  RUN_TEST(test_estimate_uses_authoritative_eligibility_and_monotonic_time);
  RUN_TEST(test_estimated_phases_duration_and_progress_are_bounded);
  RUN_TEST(test_estimate_handles_missing_data_and_timer_discontinuity);
  RUN_TEST(test_api_values_are_written_as_text_not_markup);
  RUN_TEST(test_all_schema_sections_have_consumers);
  RUN_TEST(test_configuration_and_resume_guard_suppress_tx_estimate);
  RUN_TEST(test_tx_window_pauses_requests_and_labels_estimates);
  RUN_TEST(test_timing_uncertainty_and_bounded_recovery_are_explicit);
  RUN_TEST(test_internal_navigation_waits_for_authoritative_service);
  RUN_TEST(test_current_page_enter_activation_is_request_free);
  RUN_TEST(test_recovery_policy_model_keeps_slow_retries_after_fast_limit);
  RUN_TEST(test_recovery_policy_model_valid_status_resets_every_counter);
  RUN_TEST(test_recovery_policy_model_avoids_known_window_and_overlap);
  RUN_TEST(test_navigation_priority_model_handles_idle_active_and_repeated_clicks);
  RUN_TEST(test_navigation_priority_model_recovers_and_preserves_blocks);
  return UNITY_END();
}
