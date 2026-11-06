#include <unity.h>

#include <string>

#include "ConfigurationPage.h"

namespace {
std::string page() {
  return std::string(configurationPageData(), configurationPageSize());
}
void contains(const std::string &text, const char *needle) {
  TEST_ASSERT_NOT_EQUAL(std::string::npos, text.find(needle));
}
void excludes(const std::string &text, const char *needle) {
  TEST_ASSERT_EQUAL(std::string::npos, text.find(needle));
}

struct NavigationQueueModel {
  bool navigationPending = false;
  bool pollTimerArmed = true;
  bool heartbeatTimerArmed = true;
  bool periodicReadActive = false;
  bool periodicReadSucceeded = true;
  bool waitingForPeriodic = false;
  bool mutationActive = false;
  bool mutationAborted = false;
  bool preflightComplete = false;
  bool releaseComplete = false;
  uint8_t navigations = 0;

  void begin() {
    navigationPending = true;
    pollTimerArmed = false;
    heartbeatTimerArmed = false;
    waitingForPeriodic = periodicReadActive;
  }

  bool finish(bool preflightOk, bool releaseOk, bool hasSession) {
    if (mutationActive) return false;
    if (waitingForPeriodic && !periodicReadSucceeded) return recover();
    preflightComplete = preflightOk;
    if (!preflightOk) return recover();
    releaseComplete = !hasSession || releaseOk;
    if (!releaseComplete) return recover();
    ++navigations;
    return true;
  }

  bool recover() {
    navigationPending = false;
    pollTimerArmed = true;
    heartbeatTimerArmed = true;
    return false;
  }
};
}  // namespace

void test_asset_is_complete_self_contained_and_bounded() {
  const std::string html = page();
  TEST_ASSERT_EQUAL_UINT32(html.size(), configurationPageSize());
  TEST_ASSERT_TRUE(html.size() < 49152U);
  TEST_ASSERT_EQUAL_STRING("<!doctype html>", html.substr(0, 15).c_str());
  contains(html, "</html>");
  excludes(html, "https://");
  excludes(html, "http://");
  excludes(html, "<script src=");
  excludes(html, "<link rel=");
}

void test_navigation_and_explicit_session_entry_are_separate() {
  const std::string html = page();
  contains(html, "<h1>WSPR Beacon 2.0</h1>");
  contains(html, "class=\"credit\">Developed by IW2NNV / AC2WZ");
  contains(html, "<nav class=\"nav\" aria-label=\"Primary\">");
  contains(html, "href=\"/\" id=\"dashboardLink\">Dashboard");
  contains(html, "<button type=\"button\" class=\"current\" aria-current=\"page\">Configuration</button>");
  excludes(html, "href=\"/config\" aria-current=\"page\"");
  contains(html, ".nav a{min-height:2.75rem");
  contains(html, ".nav button.current{min-height:2.75rem");
  excludes(html, "IW2NNV / AC2WZ · Configuration");
  excludes(html, "IW2NNV · JN45 · 20m");
  contains(html, "id=\"enterButton\" disabled>Enter Configuration");
  contains(html, "Opening this page is read-only");
  contains(html, "addEventListener('click',enter)");
  contains(html, "const token=await read('/api/config/v1/token')");
  contains(html, "crypto.getRandomValues(bytes)");
  contains(html, "new Uint8Array(16)");
  contains(html, "padStart(2,'0')");
  excludes(html, "Cerca reti");
  excludes(html, "Utilizza senza rete Wi-Fi");
}

void test_polling_is_serial_bounded_and_uses_only_read_only_apis() {
  const std::string html = page();
  contains(html, "POLL_MS=5000");
  contains(html, "TIMEOUT_MS=2200");
  contains(html, "const serial=job=>");
  contains(html, "queue=Promise.resolve()");
  contains(html, "method:'GET'");
  contains(html, "fetch(url");
  contains(html, "read('/api/config/v1')");
  contains(html, "read('/api/config/v1/session')");
  contains(html, "setTimeout(poll,POLL_MS)");
  contains(html, "data.schema!==1");
}

void test_enter_uses_authoritative_revision_and_security_context() {
  const std::string html = page();
  contains(html, "/api/config/v1/session/enter");
  contains(html, "expected_revision:lastSession.revision");
  contains(html, "'X-WSPR-CSRF-Token':csrfToken");
  contains(html, "headers['X-WSPR-Session']=ownerId");
  contains(html, "typeof csrfToken!=='string'||csrfToken.length!==32");
  contains(html, "sessionCertain=false");
  contains(html, "requireOwned(await mutation('/api/config/v1/session/enter'");
  contains(html, "session.mode!=='CONFIGURATION'");
  contains(html, "session.tx_admission_allowed!==false");
}

void test_heartbeat_is_bounded_serial_and_does_not_reacquire() {
  const std::string html = page();
  contains(html, "HEARTBEAT_MS=5000");
  contains(html, "RECOVERY_DELAYS=[1500,3000]");
  contains(html, "setTimeout(heartbeat,HEARTBEAT_MS)");
  contains(html, "/api/config/v1/session/heartbeat");
  contains(html, "await serial(async()=>");
  contains(html, "renew(sessionId,sessionGeneration,0)");
  contains(html, "requireOwned(await mutation('/api/config/v1/session/heartbeat',{session_id:ownerId},true,ownerId))");
  contains(html, "if(navigationPending||failureCount>=RECOVERY_DELAYS.length)return");
  contains(html, "scheduleRecovery(ownerId,generation,failureCount)");
  contains(html, "error.status===403||error.status===409");
  excludes(html, "setInterval(heartbeat");
  excludes(html, "randomId();scheduleRecovery");
}

void test_transient_heartbeat_recovery_preserves_owner_and_is_bounded() {
  const std::string html = page();
  contains(html, "function scheduleRecovery(ownerId,generation,failureCount)");
  contains(html, "setTimeout(()=>renew(ownerId,generation,failureCount+1),RECOVERY_DELAYS[failureCount])");
  contains(html, "if(sessionId!==ownerId||sessionGeneration!==generation)return");
  contains(html, "acceptSession(requireOwned(await mutation('/api/config/v1/session/heartbeat',{session_id:ownerId},true,ownerId)))");
  contains(html, "scheduleHeartbeat()");
  contains(html, "sessionCertain=false;controls()");
}

void test_exit_loss_navigation_and_resume_guard_are_conservative() {
  const std::string html = page();
  contains(html, "id=\"exitButton\" disabled>Exit Configuration");
  contains(html, "/api/config/v1/session/exit");
  contains(html, "{session_id:ownerId},true,ownerId");
  contains(html, "const session=await read('/api/config/v1/session')");
  contains(html, "if(session.web_active)throw Error('Release not confirmed')");
  contains(html, "if(!await releaseSession())throw Error('Session release not confirmed')");
  contains(html, "location.assign('/')");
  excludes(html, "Navigate anyway?");
  contains(html, "session.resume_guard===true?'TX RESUME GUARD'");
  contains(html, "addEventListener('pagehide'");
  contains(html, "stopHeartbeat();stopRecovery();sessionGeneration++");
}

void test_background_recovery_and_owner_states_are_explicit() {
  const std::string html = page();
  contains(html, "visibilitychange");
  contains(html, "document.visibilityState==='visible'&&sessionId");
  contains(html, "session.cli_active&&session.web_active?'BOTH'");
  contains(html, "Another browser owns the web session. Takeover is not permitted.");
  contains(html, "Session ownership is uncertain; parameter controls remain disabled while bounded recovery runs.");
  contains(html, "Exit is still available.");
  contains(html, "Web expiry");
  contains(html, "web_expiry_remaining_ms");
  contains(html, "if(sessionId&&session.web_active===false)clearLocalOwner()");
  excludes(html, "if(session.web_active===true)sessionCertain=true");
}

void test_all_candidate_fields_are_editable_only_with_confirmed_ownership() {
  const std::string html = page();
  contains(html, "FIELD_IDS=['callsign','locator','band','frequencyMode','frequency','offset','correction','power','interval','powerSave']");
  contains(html, "id=\"powerSave\"");
  contains(html, "Turn off firmware-controlled LEDs and OLED display after startup information is shown.");
  contains(html, "power_save:el('powerSave').value==='ON'");
  contains(html, "class=\"editable\" id=\"callsign\"");
  contains(html, "class=\"editable\" id=\"locator\"");
  contains(html, "class=\"editable\" id=\"band\"");
  contains(html, "class=\"editable\" id=\"frequencyMode\"");
  contains(html, "class=\"editable\" id=\"frequency\"");
  contains(html, "class=\"editable\" id=\"offset\"");
  contains(html, "class=\"editable\" id=\"correction\"");
  contains(html, "class=\"editable\" id=\"power\"");
  contains(html, "class=\"editable\" id=\"interval\"");
  contains(html, "lastSession.mode==='CONFIGURATION'");
  contains(html, "lastSession.tx_admission_allowed===false");
  contains(html, "FIELD_IDS.forEach(id=>el(id).disabled=!allowed)");
}

void test_capabilities_drive_options_limits_and_local_validation() {
  const std::string html = page();
  contains(html, "caps.selectable_bands");
  contains(html, "caps.frequency_modes");
  contains(html, "caps.power_dbm");
  contains(html, "caps.interval_min");
  contains(html, "caps.offset_hz");
  contains(html, "caps.correction");
  contains(html, "caps.callsign");
  contains(html, "caps.locator");
  contains(html, "band.tx_enabled!==true");
  contains(html, "frequency_min_hz");
  contains(html, "frequency_max_hz");
  contains(html, "nominal_frequency_hz");
  contains(html, "function bandChanged()");
  contains(html, "el('frequency').value=band.nominal_frequency_hz");
  contains(html, "el('band').addEventListener('change',bandChanged)");
  contains(html, "Locator must use the advertised AA00–RR99 format.");
}

void test_candidate_update_is_complete_explicit_serial_and_never_retried() {
  const std::string html = page();
  contains(html, "id=\"updateButton\" disabled>Update candidate");
  contains(html, "function candidateRequest()");
  contains(html, "expected_revision:lastConfig.revision");
  contains(html, "mutation('/api/config/v1/candidate',body,true,sessionId,'PUT')");
  contains(html, "await serial(async()=>");
  contains(html, "localDirty=!sameForm(config.candidate)");
  contains(html, "Update was not confirmed:");
  contains(html, "await refreshConfiguration(true)");
  excludes(html, "updateCandidate()}");
}

void test_local_reset_and_server_discard_have_distinct_semantics() {
  const std::string html = page();
  contains(html, "id=\"resetButton\" disabled>Reset form");
  contains(html, "function resetForm()");
  contains(html, "Local form reset to the authoritative CANDIDATE; no HTTP mutation was sent.");
  contains(html, "id=\"discardButton\" disabled>Discard changes");
  contains(html, "Discard the server CANDIDATE and replace the local form with SAVED?");
  contains(html, "mutation('/api/config/v1/discard',{expected_revision:lastConfig.revision})");
  contains(html, "Server candidate discarded; ACTIVE and SAVED are unchanged.");
}

void test_local_edits_survive_uncertain_session_and_block_save() {
  const std::string html = page();
  contains(html, "if(!localDirty)loadForm(candidate)");
  contains(html, "el('updateButton').disabled=!allowed||!localDirty");
  contains(html, "candidateMatches=!!lastConfig&&!localDirty&&sameForm(lastConfig.candidate)");
  contains(html, "el('saveButton').disabled=!allowed||!candidateMatches||!saveRequired()");
  contains(html, "id=\"saveButton\" disabled>Save and apply");
  contains(html, "Local edits not submitted");
}

void test_save_eligibility_requires_authoritative_staged_work() {
  const std::string html = page();
  contains(html, "function saveRequired(config=lastConfig)");
  contains(html, "state.candidate_dirty===true");
  contains(html, "state.active_differs_saved===true");
  contains(html, "state.runtime_synchronized===false");
  contains(html, "state.migration_pending===true");
  contains(html, "if(!canMutate()||localDirty||!lastConfig||!sameForm(lastConfig.candidate)||!saveRequired())return");
}

void test_save_requires_confirmation_and_exact_authenticated_post() {
  const std::string html = page();
  contains(html, "Save and apply WSPR configuration?");
  contains(html, "CANDIDATE will be persisted and applied.");
  contains(html, "Wi-Fi configuration will not be modified.");
  contains(html, "CONFIGURATION remains active afterward.");
  contains(html, "mutation('/api/config/v1/save',{expected_revision:expectedRevision,confirm:'save_wspr'})");
  contains(html, "'X-WSPR-CSRF-Token':csrfToken");
  contains(html, "headers['X-WSPR-Session']=ownerId");
  contains(html, "el('saveButton').addEventListener('click',saveConfiguration)");
}

void test_save_success_and_unchanged_are_truthful() {
  const std::string html = page();
  contains(html, "response.result!=='saved_applied'&&response.result!=='unchanged'");
  contains(html, "function reconciledSave(config,target)");
  contains(html, "sameSettings(saved,target)&&sameSettings(active,target)");
  contains(html, "state.runtime_synchronized===true");
  contains(html, "WSPR CANDIDATE saved and applied. Wi-Fi configuration was not modified.");
  contains(html, "no persistent write was necessary");
  contains(html, "el('saveButton').textContent='Saving…'");
  contains(html, "expectedRevision=lastConfig.revision,target=comparable(lastConfig.candidate)");
  excludes(html, "saveConfiguration(){clearLocalOwner");
  excludes(html, "saveConfiguration(){stopHeartbeat");
  excludes(html, "/api/wifi");
}

void test_save_failure_reconciles_without_retry() {
  const std::string html = page();
  contains(html, "localDirty=true;let authoritative=null");
  contains(html, "authoritative=await refreshConfiguration(true)");
  contains(html, "Save response was uncertain, but authoritative state confirms");
  contains(html, "no automatic retry was attempted");
  contains(html, "function persistedNotApplied(config,target)");
  contains(html, "persistence succeeded, but runtime application failed");
  contains(html, "persistence_failed");
  contains(html, "verification_failed");
  contains(html, "stale_revision");
  contains(html, "invalid_session");
  contains(html, "rf_unsafe");
  contains(html, "invalid_configuration");
  contains(html, "persisted_apply_failed");
  excludes(html, "setTimeout(saveConfiguration");
}

void test_required_configuration_fields_and_three_views_are_covered() {
  const std::string html = page();
  contains(html, "WSPR candidate");
  contains(html, "Callsign");
  contains(html, "Locator");
  contains(html, "Band");
  contains(html, "Frequency mode");
  contains(html, "Configured frequency");
  contains(html, "Frequency offset");
  contains(html, "Si5351A correction");
  contains(html, "Declared power");
  contains(html, "TX interval");
  contains(html, "ACTIVE / CANDIDATE / SAVED comparison");
  contains(html, "config.active");
  contains(html, "config.candidate");
  contains(html, "config.saved");
  contains(html, "state.candidate_dirty");
  contains(html, "state.active_differs_saved");
  contains(html, "state.runtime_synchronized");
}

void test_operating_session_and_stale_states_are_present() {
  const std::string html = page();
  contains(html, "CONFIGURATION — TX DISABLED");
  contains(html, "TX RESUME GUARD");
  contains(html, "session.cli_active");
  contains(html, "session.web_active");
  contains(html, "session.tx_admission_allowed");
  contains(html, "STALE_MS=10000");
  contains(html, "UNAVAILABLE_MS=30000");
  contains(html, "Data is stale; last values retained.");
  contains(html, "HTTP unavailable; last values are not current.");
}

void test_factory_reset_warning_is_clear_and_requires_restart() {
  const std::string html = page();
  contains(html, "Factory Reset erases WSPR settings and Wi-Fi credentials.");
  contains(html, "Confirm to continue.");
  contains(html, "does not restart automatically");
  contains(html, "Restart manually to provision Wi-Fi.");
  contains(html, "Restart required.");
}

void test_mobile_theme_accessibility_and_safe_dom_updates() {
  const std::string html = page();
  contains(html, "width=device-width,initial-scale=1");
  contains(html, "@media(max-width:420px)");
  contains(html, "grid-template-columns:1fr");
  contains(html, "overflow-x:hidden");
  contains(html, "min-height:2.75rem");
  contains(html, "prefers-color-scheme:dark");
  contains(html, "aria-label=\"Primary\"");
  contains(html, ".textContent=");
  contains(html, "document.createElement");
  excludes(html, "innerHTML");
  excludes(html, "eval(");
}

void test_wifi_setup_replaces_operational_editor_and_confirms_restart() {
  const std::string html = page();
  contains(html, "<h2>Wi-Fi Setup</h2>");
  contains(html, "id=\"setupButton\" disabled>Enter Wi-Fi Setup Mode");
  contains(html, "href=\"#wifi-setup\"");
  contains(html, "The current connection will be interrupted.");
  contains(html, "Saved Wi-Fi credentials are preserved");
  contains(html, "if(!confirm('Restart in AP Provisioning?");
  contains(html, "mutation('/api/config/v1/wifi/setup'");
  contains(html, "expected_revision:revision,confirm:'enter_wifi_setup'");
  contains(html, "'X-WSPR-CSRF-Token':csrfToken");
  contains(html, "el('setupButton').addEventListener('click',enterWifiSetup)");
  excludes(html, "id=\"wifiSsid\"");
  excludes(html, "id=\"wifiPassword\"");
  excludes(html, "id=\"wifiScan\"");
  excludes(html, "id=\"wifiSave\"");
  excludes(html, "/api/config/v1/wifi/save");
  excludes(html, "/api/config/v1/wifi/scan");
}

void test_pending_setup_disables_wspr_mutations_and_keeps_status_visible() {
  const std::string html = page();
  contains(html, "return !navigationPending&&!setupPending&&!!lastOk");
  contains(html, "el('restartNotice').hidden=!setupPending");
  contains(html, "el('setupButton').disabled=!allowed");
  contains(html, "el('resetButton').disabled=!allowed||!localDirty");
  contains(html, "setupPending=true;controls()");
  contains(html, "A restart may still occur; do not retry blindly.");
  contains(html, "id=\"restartNotice\" role=\"status\" hidden");
  contains(html, "id=\"setupResult\" role=\"status\"");
  contains(html, "href=\"#wspr-configuration\"");
  contains(html, "href=\"#wifi-setup\"");
}

void test_configuration_uses_stale_state_without_inventing_tx_deadlines() {
  const std::string html = page();
  contains(html, "performance.now()-lastOk<=STALE_MS");
  contains(html, "Data is stale; last values retained. Controls are temporarily unavailable.");
  contains(html, "HTTP unavailable; last values are not current. Controls are temporarily unavailable.");
  contains(html, "el('enterButton').disabled=navigationPending||!live");
  contains(html, "el('exitButton').disabled=navigationPending||!live");
  contains(html, "el('saveButton').disabled=!allowed");
  excludes(html, "next_slot_utc_ms_of_day");
  excludes(html, "\"remaining_ms\"");
}

void test_dashboard_navigation_checks_current_tx_without_a_second_prediction() {
  const std::string html = page();
  contains(html, "id=\"dashboardLink\"");
  contains(html, "id=\"navigationNotice\" role=\"status\" hidden");
  contains(html, "Navigation temporarily unavailable");
  contains(html, "event.preventDefault();const now=performance.now()");
  contains(html, "navigationBusy||now<navigationRetryAt");
  contains(html, "status=await read('/api/status')");
  contains(html, "!['RF_INHIBITED','WAITING_FOR_TIME','IDLE'].includes(tx.state)");
  contains(html, "performance.now()-started>2500");
  contains(html, "navigationRetryAt=performance.now()+15000");
  contains(html, "navigationPending=true;controls();clearTimeout(pollTimer)");
  contains(html, "const waitForPeriodic=periodicReadActive");
  contains(html, "if(waitForPeriodic&&!periodicReadSucceeded)throw Error('Periodic read failed')");
  contains(html, "const ready=await serial(async()=>");
  contains(html, "if(!await releaseSession())throw Error('Session release not confirmed')");
  contains(html, "resumePeriodic()");
  excludes(html, "next_slot_utc_ms_of_day");
}

void test_navigation_priority_stops_only_periodic_reads() {
  const std::string html = page();
  contains(html, "async function periodicRead(url)");
  contains(html, "periodicReadActive=true;periodicReadSucceeded=false");
  contains(html, "const data=await read(url);periodicReadSucceeded=true");
  contains(html, "finally{periodicReadActive=false}");
  contains(html, "if(navigationPending)return");
  contains(html, "if(!navigationPending)pollTimer=setTimeout(poll,POLL_MS)");
  contains(html, "if(!navigationPending&&sessionId&&sessionCertain)heartbeatTimer=setTimeout");
  excludes(html, "activeMutationRequest.abort");

  NavigationQueueModel model;
  model.periodicReadActive = true;
  model.mutationActive = true;
  model.begin();
  TEST_ASSERT_TRUE(model.waitingForPeriodic);
  TEST_ASSERT_FALSE(model.mutationAborted);
  TEST_ASSERT_FALSE(model.pollTimerArmed);
  TEST_ASSERT_FALSE(model.heartbeatTimerArmed);
  TEST_ASSERT_FALSE(model.finish(true, true, true));
  TEST_ASSERT_EQUAL_UINT8(0U, model.navigations);
  excludes(html, "activePeriodicRequest");
}

void test_navigation_priority_orders_preflight_release_and_recovery() {
  NavigationQueueModel withoutSession;
  withoutSession.begin();
  TEST_ASSERT_TRUE(withoutSession.finish(true, false, false));
  TEST_ASSERT_TRUE(withoutSession.preflightComplete);
  TEST_ASSERT_TRUE(withoutSession.releaseComplete);
  TEST_ASSERT_EQUAL_UINT8(1U, withoutSession.navigations);

  NavigationQueueModel withSession;
  withSession.begin();
  TEST_ASSERT_TRUE(withSession.finish(true, true, true));
  TEST_ASSERT_TRUE(withSession.releaseComplete);
  TEST_ASSERT_EQUAL_UINT8(1U, withSession.navigations);

  NavigationQueueModel preflightFailure;
  preflightFailure.begin();
  TEST_ASSERT_FALSE(preflightFailure.finish(false, false, true));
  TEST_ASSERT_TRUE(preflightFailure.pollTimerArmed);
  TEST_ASSERT_TRUE(preflightFailure.heartbeatTimerArmed);

  NavigationQueueModel periodicFailure;
  periodicFailure.periodicReadActive = true;
  periodicFailure.periodicReadSucceeded = false;
  periodicFailure.begin();
  TEST_ASSERT_FALSE(periodicFailure.finish(true, true, false));
  TEST_ASSERT_FALSE(periodicFailure.preflightComplete);
  TEST_ASSERT_TRUE(periodicFailure.pollTimerArmed);

  NavigationQueueModel releaseFailure;
  releaseFailure.begin();
  TEST_ASSERT_FALSE(releaseFailure.finish(true, false, true));
  TEST_ASSERT_TRUE(releaseFailure.preflightComplete);
  TEST_ASSERT_FALSE(releaseFailure.releaseComplete);
  TEST_ASSERT_TRUE(releaseFailure.pollTimerArmed);
}

void test_current_page_enter_activation_is_request_free() {
  const std::string html = page();
  contains(html, "<button type=\"button\" class=\"current\" aria-current=\"page\">Configuration</button>");
  excludes(html, "href=\"/config\" aria-current=\"page\"");
  excludes(html, "location.reload");
  excludes(html, "location.href='/config'");
  excludes(html, "fetch('/config'");
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_asset_is_complete_self_contained_and_bounded);
  RUN_TEST(test_navigation_and_explicit_session_entry_are_separate);
  RUN_TEST(test_polling_is_serial_bounded_and_uses_only_read_only_apis);
  RUN_TEST(test_enter_uses_authoritative_revision_and_security_context);
  RUN_TEST(test_heartbeat_is_bounded_serial_and_does_not_reacquire);
  RUN_TEST(test_transient_heartbeat_recovery_preserves_owner_and_is_bounded);
  RUN_TEST(test_exit_loss_navigation_and_resume_guard_are_conservative);
  RUN_TEST(test_background_recovery_and_owner_states_are_explicit);
  RUN_TEST(test_all_candidate_fields_are_editable_only_with_confirmed_ownership);
  RUN_TEST(test_capabilities_drive_options_limits_and_local_validation);
  RUN_TEST(test_candidate_update_is_complete_explicit_serial_and_never_retried);
  RUN_TEST(test_local_reset_and_server_discard_have_distinct_semantics);
  RUN_TEST(test_local_edits_survive_uncertain_session_and_block_save);
  RUN_TEST(test_save_eligibility_requires_authoritative_staged_work);
  RUN_TEST(test_save_requires_confirmation_and_exact_authenticated_post);
  RUN_TEST(test_save_success_and_unchanged_are_truthful);
  RUN_TEST(test_save_failure_reconciles_without_retry);
  RUN_TEST(test_required_configuration_fields_and_three_views_are_covered);
  RUN_TEST(test_operating_session_and_stale_states_are_present);
  RUN_TEST(test_factory_reset_warning_is_clear_and_requires_restart);
  RUN_TEST(test_mobile_theme_accessibility_and_safe_dom_updates);
  RUN_TEST(test_wifi_setup_replaces_operational_editor_and_confirms_restart);
  RUN_TEST(test_pending_setup_disables_wspr_mutations_and_keeps_status_visible);
  RUN_TEST(test_configuration_uses_stale_state_without_inventing_tx_deadlines);
  RUN_TEST(test_dashboard_navigation_checks_current_tx_without_a_second_prediction);
  RUN_TEST(test_navigation_priority_stops_only_periodic_reads);
  RUN_TEST(test_navigation_priority_orders_preflight_release_and_recovery);
  RUN_TEST(test_current_page_enter_activation_is_request_free);
  return UNITY_END();
}
