#include <unity.h>

#include <string.h>
#include <string>

#include "ApSetupMarker.h"
#include "ApSetupRequestJson.h"
#include "ApSetupRequestService.h"
#include "HttpTransportCore.h"
#include "ProvisioningWifiJson.h"
#include "WifiRestartController.h"

namespace {
class FakeStorage : public ConfigStorage {
 public:
  uint8_t bytes[ApSetupMarker::SIZE] = {};
  bool present = false;
  bool writeFails = false;
  bool corruptWrite = false;
  unsigned wifiWrites = 0;
  bool read(ConfigRecordType, uint8_t, uint8_t *, size_t, size_t &length) override {
    length = 0; return false;
  }
  bool write(ConfigRecordType type, uint8_t, const uint8_t *, size_t) override {
    if (type == ConfigRecordType::WIFI) ++wifiWrites;
    return true;
  }
  StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                      size_t &length) override {
    length = 0;
    if (!present) return StorageReadResult::ABSENT;
    if (capacity < sizeof(bytes)) return StorageReadResult::ERROR;
    memcpy(data, bytes, sizeof(bytes)); length = sizeof(bytes);
    return StorageReadResult::OK;
  }
  bool writeApSetupMarker(const uint8_t *data, size_t length) override {
    if (writeFails || length != sizeof(bytes)) return false;
    memcpy(bytes, data, length); present = true;
    if (corruptWrite) bytes[0] ^= 1;
    return true;
  }
};
constexpr char OWNER[] = "00112233445566778899aabbccddeeff";
constexpr char OTHER[] = "ffeeddccbbaa99887766554433221100";
constexpr char REQUEST[] =
    "{\"expected_revision\":7,\"confirm\":\"enter_wifi_setup\"}";
class FakeRfDriver : public RfDriver {
 public:
  bool detected() override { return true; }
  bool initialize(uint32_t, int32_t) override { return true; }
  bool setFrequency(uint64_t) override { return true; }
  void enableOutput() override { outputEnabled = true; }
  void disableOutput() override { outputEnabled = false; }
  bool outputEnabled = false;
};
struct Fixture {
  FakeStorage storage;
  ApSetupMarker marker;
  WifiRestartController restart;
  BeaconConfig config;
  FakeRfDriver driver;
  RfSafetyController rf;
  JTEncode encoder;
  WsprTransmitter transmitter;
  ConfigurationSessionController sessions;
  Fixture()
      : marker(storage),
        rf(driver, config.si5351ReferenceHz, config.si5351Correction),
        transmitter(config, rf, encoder) {
    BeaconSettings configured;
    strcpy(configured.callsign, "IW2NNV");
    TEST_ASSERT_TRUE(transmitter.applySettings(configured));
    TEST_ASSERT_TRUE(transmitter.begin());
  }
  ApSetupRequestResult run(const char *owner = OWNER, uint32_t now = 1001,
                           uint32_t revision = 7, bool operational = true,
                           bool factory = false, const char *body = REQUEST) {
    return ApSetupRequestService::execute(
        body, strlen(body), owner, now, revision, operational, factory,
        sessions, transmitter, marker, restart);
  }
};
HttpTransportCore::Mutation lastMutation = HttpTransportCore::Mutation::DISCARD;
unsigned mutationCalls = 0;
void mutation(void *, HttpTransportCore::Mutation operation, const char *,
              size_t, const char *, uint32_t, uint16_t &code,
              const char *&body, size_t &length) {
  lastMutation = operation; ++mutationCalls;
  static const char accepted[] = "{\"schema\":1,\"result\":\"ap_setup_pending\"}\n";
  code = 202; body = accepted; length = sizeof(accepted) - 1U;
}
std::string response(HttpTransportCore &core, const std::string &request) {
  TEST_ASSERT_TRUE(core.beginConnection(0));
  for (char value : request) core.consume(static_cast<uint8_t>(value), 1);
  std::string output;
  while (!core.complete()) {
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    TEST_ASSERT_NOT_NULL(data);
    output.append(reinterpret_cast<const char *>(data), available);
    core.consumeOutput(available, 2);
  }
  return output;
}
std::string post(const char *path, const char *body, bool csrf = true,
                 bool origin = true) {
  std::string text = std::string("POST ") + path + " HTTP/1.1\r\nHost: 192.168.4.1\r\n";
  if (origin) text += "Origin: http://192.168.4.1\r\n";
  if (csrf) text += "X-WSPR-CSRF-Token: test-token\r\n";
  text += "Content-Type: application/json\r\nContent-Length: " +
          std::to_string(strlen(body)) + "\r\n\r\n" + body;
  return text;
}
}  // namespace

void test_setup_payload_requires_revision_and_explicit_confirmation() {
  ApSetupRequest request;
  const char valid[] = "{\"expected_revision\":3,\"confirm\":\"enter_wifi_setup\"}";
  TEST_ASSERT_EQUAL_INT(ApSetupParseResult::OK,
      ApSetupRequestJson::parse(valid, sizeof(valid)-1U, request));
  TEST_ASSERT_EQUAL_UINT32(3, request.expectedRevision);
  const char missing[] = "{\"expected_revision\":3}";
  TEST_ASSERT_EQUAL_INT(ApSetupParseResult::MISSING_FIELD,
      ApSetupRequestJson::parse(missing, sizeof(missing)-1U, request));
  const char wrong[] = "{\"expected_revision\":3,\"confirm\":\"save_wifi\"}";
  TEST_ASSERT_EQUAL_INT(ApSetupParseResult::OUT_OF_RANGE,
      ApSetupRequestJson::parse(wrong, sizeof(wrong)-1U, request));
  const char duplicate[] = "{\"expected_revision\":3,\"confirm\":\"enter_wifi_setup\",\"confirm\":\"enter_wifi_setup\"}";
  TEST_ASSERT_EQUAL_INT(ApSetupParseResult::DUPLICATE_FIELD,
      ApSetupRequestJson::parse(duplicate, sizeof(duplicate)-1U, request));
  const char extra[] = "{\"expected_revision\":3,\"confirm\":\"enter_wifi_setup\",\"ssid\":\"X\"}";
  TEST_ASSERT_EQUAL_INT(ApSetupParseResult::UNKNOWN_FIELD,
      ApSetupRequestJson::parse(extra, sizeof(extra)-1U, request));
}

void test_setup_route_requires_http_security_and_marks_response() {
  HttpTransportCore core;
  core.setSecurityContext("192.168.4.1", "test-token");
  core.setMutationProvider(mutation, nullptr);
  const char body[] = "{\"expected_revision\":0,\"confirm\":\"enter_wifi_setup\"}";
  mutationCalls = 0;
  TEST_ASSERT_TRUE(response(core, post("/api/config/v1/wifi/setup", body, false))
      .find("403 Forbidden") != std::string::npos);
  core.disconnect();
  TEST_ASSERT_TRUE(response(core, post("/api/config/v1/wifi/setup", body, true, false))
      .find("403 Forbidden") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT(0, mutationCalls);
  core.disconnect();
  TEST_ASSERT_TRUE(response(core, post("/api/config/v1/wifi/setup", body))
      .find("202 Accepted") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT(1, mutationCalls);
  TEST_ASSERT_EQUAL_INT(HttpTransportCore::Mutation::ENTER_AP_SETUP, lastMutation);
  TEST_ASSERT_TRUE(core.wifiRestartResponse());
}

void test_old_operational_wifi_routes_are_absent() {
  HttpTransportCore core;
  core.setSecurityContext("192.168.4.1", "test-token");
  core.setMutationProvider(mutation, nullptr);
  for (const char *path : {"/api/config/v1/wifi", "/api/config/v1/wifi/scan"}) {
    const std::string get = std::string("GET ") + path +
        " HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n";
    TEST_ASSERT_TRUE(response(core, get).find("404 Not Found") != std::string::npos);
    core.disconnect();
  }
  for (const char *path : {"/api/config/v1/wifi/scan", "/api/config/v1/wifi/save"}) {
    TEST_ASSERT_TRUE(response(core, post(path, "{}"))
        .find("405 Method Not Allowed") != std::string::npos);
    core.disconnect();
  }
}

void test_verified_marker_and_bounded_restart_preserve_wifi_records() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  WifiRestartController restart;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.request());
  TEST_ASSERT_TRUE(restart.schedule(100));
  TEST_ASSERT_FALSE(restart.schedule(101));
  TEST_ASSERT_FALSE(restart.ready(102, false, true));
  restart.responseDelivered();
  TEST_ASSERT_FALSE(restart.ready(103, true, true));
  TEST_ASSERT_FALSE(restart.ready(103, false, false));
  TEST_ASSERT_TRUE(restart.ready(103, false, true));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, marker.read());
  TEST_ASSERT_EQUAL_UINT(0, storage.wifiWrites);
  WifiRestartController timeout;
  TEST_ASSERT_TRUE(timeout.schedule(100));
  TEST_ASSERT_FALSE(timeout.ready(100 + WifiRestartController::RESPONSE_TIMEOUT_MS - 1,
                                  true, true));
  TEST_ASSERT_TRUE(timeout.ready(100 + WifiRestartController::RESPONSE_TIMEOUT_MS,
                                 true, true));
}

void test_marker_failures_do_not_schedule_restart() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  WifiRestartController restart;
  storage.writeFails = true;
  const auto first = marker.request();
  if (first == ApSetupMarkerResult::OK) restart.schedule(0);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::STORAGE_ERROR, first);
  TEST_ASSERT_FALSE(restart.pending());
  storage.writeFails = false;
  storage.corruptWrite = true;
  const auto second = marker.request();
  if (second == ApSetupMarkerResult::OK) restart.schedule(0);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::VERIFICATION_FAILED, second);
  TEST_ASSERT_FALSE(restart.pending());
}

void test_setup_service_requires_live_owner_and_revision() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::INVALID_SESSION, fixture.run());
  TEST_ASSERT_EQUAL_INT(ConfigurationSessionResult::OK,
      fixture.sessions.acquireWeb(OWNER, 1000, fixture.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::INVALID_SESSION,
                        fixture.run(OTHER));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::STALE_REVISION,
                        fixture.run(OWNER, 1001, 8));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::INVALID_REQUEST,
                        fixture.run(OWNER, 1001, 7, true, false, "{}"));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::ACCEPTED, fixture.run());
  TEST_ASSERT_TRUE(fixture.restart.pending());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, fixture.marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::RESTART_PENDING, fixture.run());
  TEST_ASSERT_EQUAL_UINT(0, fixture.storage.wifiWrites);
}

void test_setup_service_rejects_expiry_mode_factory_and_rf_unsafe() {
  Fixture fixture;
  TEST_ASSERT_EQUAL_INT(ConfigurationSessionResult::OK,
      fixture.sessions.acquireWeb(OWNER, 1000, fixture.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::OPERATIONAL_MODE_REQUIRED,
                        fixture.run(OWNER, 1001, 7, false));
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::FACTORY_RESET_PENDING,
                        fixture.run(OWNER, 1001, 7, true, true));
  fixture.transmitter.setTxAdmissionAllowed(true);
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::RF_UNSAFE, fixture.run());
  fixture.transmitter.setTxAdmissionAllowed(false);
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::INVALID_SESSION,
      fixture.run(OWNER, 1000 + ConfigurationSessionController::WEB_SESSION_EXPIRY_MS));
  TEST_ASSERT_FALSE(fixture.restart.pending());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::ABSENT, fixture.marker.read());
}

void test_setup_service_rejects_uncertain_marker_and_prior_pending() {
  Fixture writeFailure;
  TEST_ASSERT_EQUAL_INT(ConfigurationSessionResult::OK,
      writeFailure.sessions.acquireWeb(OWNER, 1000, writeFailure.transmitter));
  writeFailure.storage.writeFails = true;
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::MARKER_WRITE_FAILED,
                        writeFailure.run());
  TEST_ASSERT_FALSE(writeFailure.restart.pending());
  Fixture verificationFailure;
  TEST_ASSERT_EQUAL_INT(ConfigurationSessionResult::OK,
      verificationFailure.sessions.acquireWeb(OWNER, 1000,
                                              verificationFailure.transmitter));
  verificationFailure.storage.corruptWrite = true;
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::MARKER_VERIFICATION_FAILED,
                        verificationFailure.run());
  TEST_ASSERT_FALSE(verificationFailure.restart.pending());
  Fixture priorPending;
  TEST_ASSERT_EQUAL_INT(ConfigurationSessionResult::OK,
      priorPending.sessions.acquireWeb(OWNER, 1000, priorPending.transmitter));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, priorPending.marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupRequestResult::ALREADY_PENDING,
                        priorPending.run());
  TEST_ASSERT_FALSE(priorPending.restart.pending());
}

void test_provisioning_open_network_parser_is_unchanged() {
  WifiCredentials credentials;
  const char open[] = "{\"ssid\":\"Guest\",\"security\":\"OPEN\",\"password\":\"\"}";
  TEST_ASSERT_EQUAL_INT(ProvisioningWifiParseResult::OK,
      ProvisioningWifiJson::parse(open, sizeof(open)-1U, credentials));
  TEST_ASSERT_TRUE(credentials.open);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_setup_payload_requires_revision_and_explicit_confirmation);
  RUN_TEST(test_setup_route_requires_http_security_and_marks_response);
  RUN_TEST(test_old_operational_wifi_routes_are_absent);
  RUN_TEST(test_verified_marker_and_bounded_restart_preserve_wifi_records);
  RUN_TEST(test_marker_failures_do_not_schedule_restart);
  RUN_TEST(test_setup_service_requires_live_owner_and_revision);
  RUN_TEST(test_setup_service_rejects_expiry_mode_factory_and_rf_unsafe);
  RUN_TEST(test_setup_service_rejects_uncertain_marker_and_prior_pending);
  RUN_TEST(test_provisioning_open_network_parser_is_unchanged);
  return UNITY_END();
}
