#include <unity.h>

#include <string>
#include <stdio.h>
#include <string.h>

#include "BootModeController.h"
#include "HttpTransportCore.h"
#include "ProvisioningPage.h"
#include "ProvisioningOperationalJson.h"
#include "ProvisioningScanJson.h"
#include "ProvisioningSecurity.h"
#include "ProvisioningWifiJson.h"

namespace {
struct RandomFixture { const uint8_t *values; size_t length; size_t offset; };
bool fill(void *context, uint8_t *destination, size_t length) {
  RandomFixture *fixture = static_cast<RandomFixture *>(context);
  if (length != 1U || fixture->offset >= fixture->length) return false;
  destination[0] = fixture->values[fixture->offset++];
  return true;
}
class FakeScanDriver : public WifiScanDriver {
 public:
  bool startAsync() override { ++starts; return startSucceeds; }
  WifiScanDriverStatus poll(uint16_t &count) override {
    ++polls; count = resultCount; return status;
  }
  bool read(uint16_t index, WifiScanEntry &entry) override {
    ++reads; if(index>=resultCount)return false; entry=results[index]; return true;
  }
  void cleanup() override { ++cleanups; }
  void add(const char *ssid,int32_t rssi,bool encrypted){
    WifiScanEntry &entry=results[resultCount++];
    strncpy(entry.ssid,ssid,sizeof(entry.ssid)-1U);
    entry.rssi=rssi;entry.encrypted=encrypted;
  }
  WifiScanEntry results[20] = {};
  uint16_t resultCount=0,starts=0,polls=0,reads=0,cleanups=0;
  bool startSucceeds=true;
  WifiScanDriverStatus status=WifiScanDriverStatus::RUNNING;
};
void feed(HttpTransportCore &core, const std::string &request) {
  TEST_ASSERT_TRUE(core.beginConnection(0));
  for (char value : request) core.consume(static_cast<uint8_t>(value), 1);
}
std::string output(HttpTransportCore &core) {
  std::string value;
  while (!core.complete()) {
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    TEST_ASSERT_NOT_NULL(data);
    value.append(reinterpret_cast<const char *>(data), available);
    core.consumeOutput(available, 2);
  }
  return value;
}
bool scanBody(void *,uint32_t,const char *&body,size_t &length){
  static const char value[]="{\"schema\":1,\"state\":\"IDLE\"}\n";
  body=value;length=sizeof(value)-1U;return true;
}
uint8_t scanMutations=0;
uint8_t operationalMutations=0;
void mutation(void *,HttpTransportCore::Mutation operation,const char *,size_t,
              const char *,uint32_t,uint16_t &code,const char *&body,size_t &length){
  static const char value[]="{\"schema\":1,\"result\":\"started\"}\n";
  if(operation==HttpTransportCore::Mutation::START_SCAN)++scanMutations;
  if(operation==HttpTransportCore::Mutation::ENTER_AP_OPERATIONAL)
    ++operationalMutations;
  code=202;body=value;length=sizeof(value)-1U;
}
}

void setUp() {}
void tearDown() {}

void test_boot_without_credentials_requests_and_latches_ap() {
  BootModeController controller;
  controller.begin(false, 100);
  TEST_ASSERT_TRUE(controller.enterApRequested());
  controller.apStartResult(true);
  TEST_ASSERT_TRUE(controller.provisioning());
  controller.update(true, false, 50000);
  TEST_ASSERT_TRUE(controller.provisioning());
}

void test_initial_sta_connects_or_times_out_wrap_safely() {
  BootModeController connected;
  connected.begin(true, 20);
  connected.update(true, false, 21);
  TEST_ASSERT_TRUE(connected.normal());
  BootModeController timeout;
  const uint32_t start = UINT32_MAX - 5000U;
  timeout.begin(true, start);
  timeout.update(false, false, start + 14999U);
  TEST_ASSERT_FALSE(timeout.enterApRequested());
  timeout.update(false, false, start + 15000U);
  TEST_ASSERT_TRUE(timeout.enterApRequested());
}

void test_ap_failure_is_fail_closed() {
  BootModeController controller;
  controller.begin(false, 0);
  controller.apStartResult(false);
  TEST_ASSERT_TRUE(controller.failClosed());
}

void test_ap_operational_transition_is_explicit_boot_only_and_one_way() {
  BootModeController controller;
  controller.begin(false, 0);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ApOperationalRequestResult::REJECTED),
      static_cast<uint8_t>(controller.requestApOperational()));
  controller.apStartResult(true);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ApOperationalRequestResult::ACCEPTED),
      static_cast<uint8_t>(controller.requestApOperational()));
  TEST_ASSERT_TRUE(controller.provisioning());
  TEST_ASSERT_TRUE(controller.apOperationalPending());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ApOperationalRequestResult::ALREADY_PENDING),
      static_cast<uint8_t>(controller.requestApOperational()));
  TEST_ASSERT_TRUE(controller.completeApOperationalTransition());
  TEST_ASSERT_TRUE(controller.apOperational());
  TEST_ASSERT_TRUE(controller.rfOperationAllowed());
  TEST_ASSERT_TRUE(controller.timeAuthorizationAllowed(
      WsprTimeAuthorization::GPS_PRECISE));
  TEST_ASSERT_FALSE(controller.timeAuthorizationAllowed(
      WsprTimeAuthorization::NTP_EXPERIMENTAL));
  TEST_ASSERT_TRUE(controller.httpNetworkAvailable(false, true));
  TEST_ASSERT_FALSE(controller.httpNetworkAvailable(false, false));
  controller.update(true, false, 50000);
  TEST_ASSERT_TRUE(controller.apOperational());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ApOperationalRequestResult::REJECTED),
      static_cast<uint8_t>(controller.requestApOperational()));
  controller.begin(true, 60000);
  TEST_ASSERT_FALSE(controller.apOperational());
  TEST_ASSERT_FALSE(controller.apOperationalPending());
  controller.update(true, false, 60001);
  TEST_ASSERT_TRUE(controller.rfOperationAllowed());
  TEST_ASSERT_TRUE(controller.timeAuthorizationAllowed(
      WsprTimeAuthorization::NTP_EXPERIMENTAL));
  TEST_ASSERT_TRUE(controller.httpNetworkAvailable(true, false));
}

void test_ap_operational_payload_is_exact() {
  const char valid[] = " { \"confirm\" : \"ap_operational\" } \n";
  TEST_ASSERT_TRUE(ProvisioningOperationalJson::valid(
      valid, sizeof(valid) - 1U));
  const char wrong[] = "{\"confirm\":\"other\"}";
  const char extra[] =
      "{\"confirm\":\"ap_operational\",\"extra\":true}";
  TEST_ASSERT_FALSE(ProvisioningOperationalJson::valid(
      wrong, sizeof(wrong) - 1U));
  TEST_ASSERT_FALSE(ProvisioningOperationalJson::valid(
      extra, sizeof(extra) - 1U));
}

void test_password_is_eight_digits_stable_and_rejection_sampled() {
  const uint8_t values[] = {250, 0, 1, 2, 3, 4, 5, 6, 7};
  RandomFixture fixture{values, sizeof(values), 0};
  char password[9];
  TEST_ASSERT_TRUE(ProvisioningSecurity::generateNumericPassword(
      fill, &fixture, password));
  TEST_ASSERT_EQUAL_STRING("01234567", password);
}

void test_wifi_json_accepts_escaped_printable_values_and_rejects_unknown() {
  WifiCredentials credentials;
  const char valid[] = "{\"ssid\":\"A\\\\B\\\"C\",\"password\":\"12345678\"}";
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ProvisioningWifiParseResult::OK),
      static_cast<uint8_t>(ProvisioningWifiJson::parse(
          valid, sizeof(valid) - 1U, credentials)));
  TEST_ASSERT_EQUAL_STRING("A\\B\"C", credentials.ssid);
  credentials.clearPassword();
  const char invalid[] = "{\"ssid\":\"A\",\"password\":\"12345678\",\"x\":1}";
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(ProvisioningWifiParseResult::UNKNOWN_FIELD),
      static_cast<uint8_t>(ProvisioningWifiJson::parse(
          invalid, sizeof(invalid) - 1U, credentials)));
}

void test_provisioning_profile_isolates_normal_routes_and_checks_host() {
  HttpTransportCore core;
  core.setProfile(HttpTransportCore::Profile::PROVISIONING);
  core.setSecurityContext("192.168.4.1", "0123456789abcdef0123456789abcdef");
  feed(core, "GET /api/status HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(core).find("404 Not Found") != std::string::npos);
  core.disconnect();
  feed(core, "GET / HTTP/1.1\r\nHost: wrong\r\n\r\n");
  TEST_ASSERT_TRUE(output(core).find("403 Forbidden") != std::string::npos);
  core.disconnect();
  feed(core, "GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  const std::string page = output(core);
  TEST_ASSERT_TRUE(page.find("200 OK") != std::string::npos);
  TEST_ASSERT_TRUE(page.find("AP Provisioning — TX inhibited") != std::string::npos);
}

void test_scan_is_manual_bounded_sorted_deduplicated_and_filters_ssids() {
  WifiScanController controller;
  FakeScanDriver driver;
  TEST_ASSERT_EQUAL_UINT16(0,driver.starts);
  driver.add("weak",-80,true);
  driver.add("same",-70,true);
  driver.add("same",-5,true);
  driver.add("same",-6,false);
  driver.add("",-10,true);
  driver.add("bad\nssid",-9,true);
  for(int i=0;i<12;++i){char name[8];snprintf(name,sizeof(name),"n%02d",i);driver.add(name,-20-i,true);}
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::STARTED),
      static_cast<uint8_t>(controller.start(driver,100)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::ALREADY_SCANNING),
      static_cast<uint8_t>(controller.start(driver,101)));
  TEST_ASSERT_EQUAL_UINT16(1,driver.starts);
  driver.status=WifiScanDriverStatus::COMPLETE;
  controller.update(driver,200);
  const WifiScanSnapshot &snapshot=controller.snapshot();
  TEST_ASSERT_EQUAL_UINT8(10,snapshot.count);
  TEST_ASSERT_EQUAL_STRING("same",snapshot.entries[0].ssid);
  for(uint8_t i=1;i<snapshot.count;++i)
    TEST_ASSERT_TRUE(snapshot.entries[i-1].rssi>=snapshot.entries[i].rssi);
  uint8_t secureSame=0,openSame=0;
  for(uint8_t i=0;i<snapshot.count;++i){
    if(strcmp(snapshot.entries[i].ssid,"same")==0)
      snapshot.entries[i].encrypted?++secureSame:++openSame;
  }
  TEST_ASSERT_EQUAL_UINT8(1,secureSame);
  TEST_ASSERT_EQUAL_UINT8(1,openSame);
  TEST_ASSERT_EQUAL_UINT16(1,driver.cleanups);
}

void test_scan_timeout_keeps_driver_locked_until_late_cleanup() {
  WifiScanController controller;
  FakeScanDriver driver;
  controller.start(driver,UINT32_MAX-1000U);
  controller.update(driver,UINT32_MAX-1000U+WifiScanController::TIMEOUT_MS);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanState::FAILED),
      static_cast<uint8_t>(controller.snapshot().state));
  TEST_ASSERT_TRUE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::DRIVER_BUSY),
      static_cast<uint8_t>(controller.start(driver,20)));
  driver.status=WifiScanDriverStatus::COMPLETE;
  controller.update(driver,UINT32_MAX-1000U+WifiScanController::TIMEOUT_MS+30U);
  TEST_ASSERT_FALSE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT16(1,driver.cleanups);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanState::FAILED),
      static_cast<uint8_t>(controller.snapshot().state));
}

void test_scan_recovery_has_bounded_polls_and_blocks_unverified_driver() {
  WifiScanController controller;
  FakeScanDriver driver;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::STARTED),
      static_cast<uint8_t>(controller.start(driver, 0)));
  controller.update(driver, WifiScanController::TIMEOUT_MS);
  TEST_ASSERT_TRUE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::TIMEOUT),
      static_cast<uint8_t>(controller.snapshot().error));
  for (uint8_t attempt = 0; attempt < WifiScanController::MAX_RECOVERY_POLLS;
       ++attempt) controller.update(driver, WifiScanController::TIMEOUT_MS + 1U);
  const uint16_t polls = driver.polls;
  controller.update(driver, WifiScanController::TIMEOUT_MS +
      WifiScanController::RECOVERY_TIMEOUT_MS + 1U);
  TEST_ASSERT_EQUAL_UINT16(polls, driver.polls);
  TEST_ASSERT_TRUE(controller.snapshot().recoveryBlocked);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::RECOVERY_EXHAUSTED),
      static_cast<uint8_t>(controller.snapshot().error));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::DRIVER_BUSY),
      static_cast<uint8_t>(controller.start(driver, 20000)));
  driver.status = WifiScanDriverStatus::COMPLETE;
  controller.update(driver, 20001);
  TEST_ASSERT_EQUAL_UINT16(polls, driver.polls);
}

void test_completed_driver_after_deadline_is_cleaned_without_results() {
  WifiScanController controller;
  FakeScanDriver driver;
  driver.add("late", -30, true);
  controller.start(driver, 0);
  driver.status = WifiScanDriverStatus::COMPLETE;
  controller.update(driver, WifiScanController::TIMEOUT_MS);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanState::FAILED),
      static_cast<uint8_t>(controller.snapshot().state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::TIMEOUT),
      static_cast<uint8_t>(controller.snapshot().error));
  TEST_ASSERT_EQUAL_UINT8(0, controller.snapshot().count);
  TEST_ASSERT_FALSE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT16(1, driver.cleanups);
}

void test_failed_driver_can_complete_during_bounded_recovery() {
  WifiScanController controller;
  FakeScanDriver driver;
  controller.start(driver, 0);
  driver.status = WifiScanDriverStatus::FAILED;
  controller.update(driver, 1);
  TEST_ASSERT_TRUE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::DRIVER_FAILED),
      static_cast<uint8_t>(controller.snapshot().error));
  controller.update(driver, 2);
  driver.status = WifiScanDriverStatus::COMPLETE;
  controller.update(driver, 3);
  TEST_ASSERT_FALSE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT16(1, driver.cleanups);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::STARTED),
      static_cast<uint8_t>(controller.start(driver, 4)));
}

void test_persistent_failed_driver_stops_after_bounded_attempts() {
  WifiScanController controller;
  FakeScanDriver driver;
  controller.start(driver, 0);
  driver.status = WifiScanDriverStatus::FAILED;
  for (uint8_t attempt = 0; attempt <= WifiScanController::MAX_RECOVERY_POLLS + 1U;
       ++attempt) controller.update(driver, attempt + 1U);
  TEST_ASSERT_TRUE(controller.snapshot().recoveryBlocked);
  TEST_ASSERT_TRUE(controller.snapshot().driverBusy);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::RECOVERY_EXHAUSTED),
      static_cast<uint8_t>(controller.snapshot().error));
  TEST_ASSERT_EQUAL_UINT16(0, driver.cleanups);
  const uint16_t polls = driver.polls;
  controller.update(driver, 1000);
  TEST_ASSERT_EQUAL_UINT16(polls, driver.polls);
}

void test_ap_restore_success_and_failure_are_distinct() {
  WifiScanController controller;
  FakeScanDriver driver;
  controller.start(driver, 0);
  driver.status = WifiScanDriverStatus::COMPLETE;
  controller.update(driver, 1);
  controller.setApRestored(true);
  TEST_ASSERT_TRUE(controller.snapshot().apRestored);
  TEST_ASSERT_FALSE(controller.snapshot().recoveryBlocked);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::STARTED),
      static_cast<uint8_t>(controller.start(driver, 2)));
  driver.status = WifiScanDriverStatus::COMPLETE;
  controller.update(driver, 3);
  controller.blockApRestore();
  TEST_ASSERT_FALSE(controller.snapshot().apRestored);
  TEST_ASSERT_TRUE(controller.snapshot().recoveryBlocked);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanError::AP_RESTORE_FAILED),
      static_cast<uint8_t>(controller.snapshot().error));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiScanStartResult::DRIVER_BUSY),
      static_cast<uint8_t>(controller.start(driver, 4)));
}

void test_empty_results_snapshot_age_stale_and_json_escape() {
  WifiScanController controller;
  FakeScanDriver empty;
  controller.start(empty,1);empty.status=WifiScanDriverStatus::COMPLETE;
  controller.update(empty,10);
  TEST_ASSERT_EQUAL_UINT8(0,controller.snapshot().count);
  FakeScanDriver escaped;
  escaped.add("quote\"slash\\",-42,true);
  controller.start(escaped,20);escaped.status=WifiScanDriverStatus::COMPLETE;
  controller.update(escaped,30);
  char output[ProvisioningScanJson::MAX_RESPONSE_BYTES+1U];size_t length=0;
  TEST_ASSERT_TRUE(ProvisioningScanJson::serialize(
      controller.snapshot(),30+WifiScanController::STALE_MS,
      output,sizeof(output),length));
  TEST_ASSERT_NOT_NULL(strstr(output,"quote\\\"slash\\\\"));
  TEST_ASSERT_NOT_NULL(strstr(output,"\"stale\":true"));
}

void test_scan_endpoints_are_profile_isolated_and_security_protected() {
  const char *token="0123456789abcdef0123456789abcdef";
  HttpTransportCore normal;
  normal.setSecurityContext("192.168.4.1",token);
  feed(normal,"GET /api/provision/v1/scan HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(normal).find("404 Not Found")!=std::string::npos);

  HttpTransportCore provision;
  provision.setProfile(HttpTransportCore::Profile::PROVISIONING);
  provision.setSecurityContext("192.168.4.1",token);
  provision.setScanBodyProvider(scanBody,nullptr);
  provision.setMutationProvider(mutation,nullptr);
  feed(provision,"GET /api/provision/v1/scan HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(provision).find("\"state\":\"IDLE\"")!=std::string::npos);
  provision.disconnect();scanMutations=0;
  feed(provision,"POST /api/provision/v1/scan HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
  TEST_ASSERT_TRUE(output(provision).find("403 Forbidden")!=std::string::npos);
  TEST_ASSERT_EQUAL_UINT8(0,scanMutations);
  provision.disconnect();
  const std::string request=std::string("POST /api/provision/v1/scan HTTP/1.1\r\nHost: 192.168.4.1\r\nOrigin: http://192.168.4.1\r\nX-WSPR-CSRF-Token: ")+token+"\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}";
  feed(provision,request);
  TEST_ASSERT_TRUE(output(provision).find("202 Accepted")!=std::string::npos);
  TEST_ASSERT_EQUAL_UINT8(1,scanMutations);
}

void test_ap_operational_endpoint_and_profile_are_isolated() {
  const char *token="0123456789abcdef0123456789abcdef";
  HttpTransportCore provision;
  provision.setProfile(HttpTransportCore::Profile::PROVISIONING);
  provision.setSecurityContext("192.168.4.1",token);
  provision.setMutationProvider(mutation,nullptr);
  operationalMutations=0;
  const std::string body="{\"confirm\":\"ap_operational\"}";
  const std::string request=std::string(
      "POST /api/provision/v1/operational HTTP/1.1\r\n"
      "Host: 192.168.4.1\r\nOrigin: http://192.168.4.1\r\n"
      "X-WSPR-CSRF-Token: ")+token+
      "\r\nContent-Type: application/json\r\nContent-Length: "+
      std::to_string(body.size())+"\r\n\r\n"+body;
  feed(provision,request);
  TEST_ASSERT_TRUE(output(provision).find("202 Accepted")!=std::string::npos);
  TEST_ASSERT_TRUE(provision.apOperationalTransitionResponse());
  TEST_ASSERT_EQUAL_UINT8(1,operationalMutations);

  HttpTransportCore operational;
  operational.setProfile(HttpTransportCore::Profile::AP_OPERATIONAL);
  operational.setSecurityContext("192.168.4.1",token);
  feed(operational,"GET / HTTP/1.1\r\nHost: wrong\r\n\r\n");
  TEST_ASSERT_TRUE(output(operational).find("403 Forbidden")!=std::string::npos);
  operational.disconnect();
  feed(operational,"GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(operational).find("WSPR Beacon 2.0")!=std::string::npos);
  operational.disconnect();
  feed(operational,
       "GET /config HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(operational).find("200 OK")!=std::string::npos);
  operational.disconnect();
  feed(operational,
       "GET /api/provision/v1/token HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n");
  TEST_ASSERT_TRUE(output(operational).find("404 Not Found")!=std::string::npos);
}

void test_page_scans_only_on_click_and_preserves_manual_form() {
  const std::string page(provisioningPageData(),provisioningPageSize());
  TEST_ASSERT_TRUE(page.find("<h1>WSPR Beacon 2.0</h1>")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Developed by IW2NNV / AC2WZ")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Scan for networks")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("scanButton.addEventListener('click'")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("setInterval") == std::string::npos);
  TEST_ASSERT_TRUE(page.find("window.onload") == std::string::npos);
  TEST_ASSERT_TRUE(page.find("replaceChildren")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("document.getElementById('ssid').value=network.ssid")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("document.getElementById('password').value=network") == std::string::npos);
  TEST_ASSERT_TRUE(page.find("Manual SSID entry remains available")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("save outcome is unknown")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Use without Wi-Fi network")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("AP Provisioning — TX inhibited")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("RF output is disabled in AP Provisioning")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Saving credentials does not connect to the station network immediately")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("WSPR TX may become eligible only with GPS_PRECISE")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Manual TX permission never overrides safety gates")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("Cerca reti")==std::string::npos);
  TEST_ASSERT_TRUE(page.find("Utilizza senza rete Wi-Fi")==std::string::npos);
  TEST_ASSERT_TRUE(page.find("WSPR TX will remain disabled")==std::string::npos);
  TEST_ASSERT_TRUE(page.find("operationalButton.addEventListener('click'")!=std::string::npos);
  TEST_ASSERT_TRUE(page.find("\"confirm\":\"ap_operational\"")!=std::string::npos);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_boot_without_credentials_requests_and_latches_ap);
  RUN_TEST(test_initial_sta_connects_or_times_out_wrap_safely);
  RUN_TEST(test_ap_failure_is_fail_closed);
  RUN_TEST(test_ap_operational_transition_is_explicit_boot_only_and_one_way);
  RUN_TEST(test_ap_operational_payload_is_exact);
  RUN_TEST(test_password_is_eight_digits_stable_and_rejection_sampled);
  RUN_TEST(test_wifi_json_accepts_escaped_printable_values_and_rejects_unknown);
  RUN_TEST(test_provisioning_profile_isolates_normal_routes_and_checks_host);
  RUN_TEST(test_scan_is_manual_bounded_sorted_deduplicated_and_filters_ssids);
  RUN_TEST(test_scan_timeout_keeps_driver_locked_until_late_cleanup);
  RUN_TEST(test_scan_recovery_has_bounded_polls_and_blocks_unverified_driver);
  RUN_TEST(test_completed_driver_after_deadline_is_cleaned_without_results);
  RUN_TEST(test_failed_driver_can_complete_during_bounded_recovery);
  RUN_TEST(test_persistent_failed_driver_stops_after_bounded_attempts);
  RUN_TEST(test_ap_restore_success_and_failure_are_distinct);
  RUN_TEST(test_empty_results_snapshot_age_stale_and_json_escape);
  RUN_TEST(test_scan_endpoints_are_profile_isolated_and_security_protected);
  RUN_TEST(test_ap_operational_endpoint_and_profile_are_isolated);
  RUN_TEST(test_page_scans_only_on_click_and_preserves_manual_form);
  return UNITY_END();
}
