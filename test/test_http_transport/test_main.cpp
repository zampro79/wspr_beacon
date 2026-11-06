#include <unity.h>

#include <errno.h>
#include <cstring>
#include <string>
#include <vector>

#include "DashboardPage.h"
#include "ConfigurationPage.h"
#include "HttpServiceGate.h"
#include "HttpNonBlockingWriter.h"
#include "HttpTransportCore.h"
#include "HttpWritePlanner.h"
#include "TimeService.h"
#include "WsprScheduler.h"

namespace {
struct StatusProviderProbe {
  const char *body = nullptr;
  size_t length = 0;
  uint16_t calls = 0;
  uint32_t nowMillis = 0;
  bool succeed = true;
};

struct MutationProbe {
  uint16_t calls = 0;
  HttpTransportCore::Mutation mutation = HttpTransportCore::Mutation::DISCARD;
  std::string body;
  std::string session;
};

struct SocketSendProbe {
  std::vector<int> results;
  std::vector<int> errors;
  std::string submitted;
  size_t calls = 0;
  int lastDescriptor = -1;
  size_t lastLength = 0;
};

int simulatedSend(void *context, int socketDescriptor, const uint8_t *data,
                  size_t length, int &errorCode) {
  SocketSendProbe &probe = *static_cast<SocketSendProbe *>(context);
  const size_t index = probe.calls++;
  probe.lastDescriptor = socketDescriptor;
  probe.lastLength = length;
  if (index >= probe.results.size()) {
    errorCode = EIO;
    return -1;
  }
  errorCode = index < probe.errors.size() ? probe.errors[index] : 0;
  const int result = probe.results[index];
  if (result > 0 && static_cast<size_t>(result) <= length)
    probe.submitted.append(reinterpret_cast<const char *>(data),
                           static_cast<size_t>(result));
  return result;
}

void performMutation(void *context, HttpTransportCore::Mutation mutation,
                     const char *body, size_t length, const char *session,
                     uint32_t,
                     uint16_t &statusCode, const char *&responseBody,
                     size_t &responseLength) {
  static const char RESPONSE[] = "{\"schema\":1,\"revision\":2}\n";
  MutationProbe &probe = *static_cast<MutationProbe *>(context);
  ++probe.calls;
  probe.mutation = mutation;
  probe.body.assign(body, length);
  probe.session = session == nullptr ? "" : session;
  statusCode = 200;
  responseBody = RESPONSE;
  responseLength = sizeof(RESPONSE) - 1U;
}

bool provideStatus(void *context, uint32_t nowMillis, const char *&body,
                   size_t &length) {
  StatusProviderProbe &probe = *static_cast<StatusProviderProbe *>(context);
  ++probe.calls;
  probe.nowMillis = nowMillis;
  if (!probe.succeed) return false;
  body = probe.body;
  length = probe.length;
  return true;
}

void feed(HttpTransportCore &core, const std::string &text, uint32_t now = 1) {
  for (char value : text) {
    core.consume(static_cast<uint8_t>(value), now++);
  }
}

void feedAt(HttpTransportCore &core, const std::string &text, uint32_t now) {
  for (char value : text) {
    core.consume(static_cast<uint8_t>(value), now);
  }
}

std::string drain(HttpTransportCore &core, size_t chunk = SIZE_MAX) {
  std::string result;
  uint32_t now = 100;
  while (core.responseReady()) {
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    TEST_ASSERT_NOT_NULL(data);
    const size_t count = available < chunk ? available : chunk;
    result.append(reinterpret_cast<const char *>(data), count);
    core.consumeOutput(count, now++);
  }
  return result;
}

void assertContentLengthMatches(const std::string &response) {
  const std::string label = "Content-Length: ";
  const size_t valueStart = response.find(label);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, valueStart);
  const size_t valueEnd = response.find("\r\n", valueStart);
  const size_t bodyStart = response.find("\r\n\r\n");
  TEST_ASSERT_NOT_EQUAL(std::string::npos, valueEnd);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, bodyStart);
  const size_t numberStart = valueStart + label.size();
  const size_t declared = static_cast<size_t>(
      std::stoul(response.substr(numberStart, valueEnd - numberStart)));
  TEST_ASSERT_EQUAL_UINT32(declared, response.size() - bodyStart - 4U);
}

std::string responseBody(const std::string &response) {
  const size_t bodyStart = response.find("\r\n\r\n");
  TEST_ASSERT_NOT_EQUAL(std::string::npos, bodyStart);
  return response.substr(bodyStart + 4U);
}

HttpServiceGateInputs safeGate() {
  HttpServiceGateInputs inputs;
  inputs.wifiConnected = true;
  inputs.rfOperational = true;
  inputs.authorization = WsprTimeAuthorization::GPS_PRECISE;
  inputs.nextSlotKnown = true;
  inputs.millisUntilSlot = HttpServiceGate::SLOT_GUARD_MS + 1UL;
  return inputs;
}

TimeSnapshot validTime(uint32_t utcMillis) {
  TimeService service(TimeServiceConfig(5000UL, 5000UL));
  TimeReference reference;
  reference.utcMillisOfDay = utcMillis;
  reference.utcValid = true;
  reference.quality = TimeQuality::PRECISE;
  service.update(TimeSource::GPS, reference, 0);
  return service.snapshot(0);
}
}  // namespace

void test_valid_root_get() {
  HttpTransportCore core;
  TEST_ASSERT_TRUE(core.beginConnection(0));
  feed(core, "GET / HTTP/1.1\r\nHost: beacon\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  const std::string response = drain(core);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, response.find("HTTP/1.1 200 OK"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("WSPR Beacon 2.0"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("fetch('/api/status'"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, response.find("</html>"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("Connection: close"));
  assertContentLengthMatches(response);
}

void test_root_header_is_ram_and_dashboard_body_is_program_memory() {
  HttpTransportCore core;
  TEST_ASSERT_TRUE(core.beginConnection(0));
  feed(core, "GET / HTTP/1.1\r\nHost: beacon\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpBodyStorage::RAM),
                          static_cast<uint8_t>(core.outputStorage()));
  size_t headerBytes = 0;
  TEST_ASSERT_NOT_NULL(core.outputData(headerBytes));
  TEST_ASSERT_TRUE(headerBytes > 0);
  core.consumeOutput(headerBytes, 100);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpBodyStorage::PROGRAM_MEMORY),
      static_cast<uint8_t>(core.outputStorage()));
  size_t bodyBytes = 0;
  TEST_ASSERT_NOT_NULL(core.outputData(bodyBytes));
  TEST_ASSERT_EQUAL_UINT32(dashboardPageSize(), bodyBytes);
}

void test_valid_configuration_page_get_uses_program_memory() {
  HttpTransportCore core;
  TEST_ASSERT_TRUE(core.beginConnection(0));
  feed(core, "GET /config HTTP/1.1\r\nHost: beacon\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpBodyStorage::RAM),
                          static_cast<uint8_t>(core.outputStorage()));
  size_t headerBytes = 0;
  const uint8_t *header = core.outputData(headerBytes);
  TEST_ASSERT_NOT_NULL(header);
  std::string response(reinterpret_cast<const char *>(header), headerBytes);
  core.consumeOutput(headerBytes, 100);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpBodyStorage::PROGRAM_MEMORY),
      static_cast<uint8_t>(core.outputStorage()));
  size_t bodyBytes = 0;
  TEST_ASSERT_NOT_NULL(core.outputData(bodyBytes));
  TEST_ASSERT_EQUAL_UINT32(configurationPageSize(), bodyBytes);
  response += drain(core, 127);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("WSPR Beacon 2.0 — Configuration"));
  assertContentLengthMatches(response);
}

void test_static_pages_negotiate_deterministic_gzip_assets() {
  HttpTransportCore dashboard;
  TEST_ASSERT_TRUE(dashboard.beginConnection(0));
  feed(dashboard, "GET / HTTP/1.1\r\nHost: beacon\r\n"
                  "Accept-Encoding: br, gzip\r\n\r\n");
  const std::string dashboardResponse = drain(dashboard, 127);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        dashboardResponse.find("Content-Encoding: gzip\r\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        dashboardResponse.find("Vary: Accept-Encoding\r\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        dashboardResponse.find("Content-Type: text/html; charset=utf-8\r\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        dashboardResponse.find("Cache-Control: no-store\r\n"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        dashboardResponse.find("Connection: close\r\n"));
  assertContentLengthMatches(dashboardResponse);
  const std::string dashboardBody = responseBody(dashboardResponse);
  TEST_ASSERT_EQUAL_UINT32(dashboardPageGzipSize(), dashboardBody.size());
  TEST_ASSERT_EQUAL_MEMORY(dashboardPageGzipData(), dashboardBody.data(),
                           dashboardBody.size());
  TEST_ASSERT_EQUAL_HEX8(0x1f, static_cast<uint8_t>(dashboardBody[0]));
  TEST_ASSERT_EQUAL_HEX8(0x8b, static_cast<uint8_t>(dashboardBody[1]));

  HttpTransportCore configuration;
  TEST_ASSERT_TRUE(configuration.beginConnection(0));
  feed(configuration, "GET /config HTTP/1.1\r\nHost: beacon\r\n"
                      "Accept-Encoding: GZip; q=0.5\r\n\r\n");
  const std::string configurationResponse = drain(configuration, 113);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        configurationResponse.find("Content-Encoding: gzip\r\n"));
  assertContentLengthMatches(configurationResponse);
  const std::string configurationBody = responseBody(configurationResponse);
  TEST_ASSERT_EQUAL_UINT32(configurationPageGzipSize(),
                           configurationBody.size());
  TEST_ASSERT_EQUAL_MEMORY(configurationPageGzipData(),
                           configurationBody.data(),
                           configurationBody.size());
}

void test_static_pages_preserve_identity_fallback() {
  HttpTransportCore unsupported;
  unsupported.beginConnection(0);
  feed(unsupported, "GET / HTTP/1.1\r\nHost: beacon\r\n"
                    "Accept-Encoding: br, *;q=1\r\n\r\n");
  const std::string unsupportedResponse = drain(unsupported);
  TEST_ASSERT_EQUAL(std::string::npos,
                    unsupportedResponse.find("Content-Encoding:"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        unsupportedResponse.find("Vary: Accept-Encoding\r\n"));
  TEST_ASSERT_EQUAL_STRING_LEN(dashboardPageData(),
                               responseBody(unsupportedResponse).data(),
                               dashboardPageSize());

  HttpTransportCore rejected;
  rejected.beginConnection(0);
  feed(rejected, "GET /config HTTP/1.1\r\nHost: beacon\r\n"
                 "Accept-Encoding: gzip;q=0, br\r\n\r\n");
  const std::string rejectedResponse = drain(rejected);
  TEST_ASSERT_EQUAL(std::string::npos,
                    rejectedResponse.find("Content-Encoding:"));
  TEST_ASSERT_EQUAL_UINT32(configurationPageSize(),
                           responseBody(rejectedResponse).size());
}

void test_json_responses_are_never_gzip_encoded() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\nHost: beacon\r\n"
             "Accept-Encoding: gzip\r\n\r\n");
  const std::string response = drain(core);
  TEST_ASSERT_EQUAL(std::string::npos, response.find("Content-Encoding:"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("application/json"));
  assertContentLengthMatches(response);
}

void test_valid_status_get() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\nHost: beacon\r\n\r\n");
  const std::string response = drain(core);
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("application/json"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("\"status\":\"available\""));
  assertContentLengthMatches(response);
}

void test_status_provider_is_on_demand_and_response_is_bounded() {
  static const char body[] =
      "{\"schema\":1,\"identity\":{},\"tx\":{},\"time\":{},"
      "\"network\":{},\"gps\":{},\"modules\":{},\"system\":{}}\n";
  StatusProviderProbe probe;
  probe.body = body;
  probe.length = sizeof(body) - 1U;
  HttpTransportCore core;
  core.setStatusBodyProvider(provideStatus, &probe);
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\nHost: beacon\r\n");
  TEST_ASSERT_EQUAL_UINT16(0, probe.calls);
  feed(core, "\r\n", 50);
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_TRUE(probe.nowMillis >= 50);
  const std::string response = drain(core, 17);
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, response.find(body));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("Cache-Control: no-store"));
  assertContentLengthMatches(response);
}

void test_status_provider_failure_returns_complete_503_json() {
  StatusProviderProbe probe;
  probe.succeed = false;
  HttpTransportCore core;
  core.setStatusBodyProvider(provideStatus, &probe);
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\nHost: beacon\r\n\r\n");
  const std::string response = drain(core);
  TEST_ASSERT_EQUAL_UINT16(503, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("\"error\":\"status_unavailable\""));
  assertContentLengthMatches(response);
}

void test_valid_config_get_uses_its_own_read_only_provider() {
  static const char configBody[] =
      "{\"schema\":1,\"revision\":4,\"active\":{},\"candidate\":{},"
      "\"saved\":{},\"state\":{},\"capabilities\":{}}\n";
  StatusProviderProbe statusProbe;
  statusProbe.succeed = false;
  StatusProviderProbe configProbe;
  configProbe.body = configBody;
  configProbe.length = sizeof(configBody) - 1U;
  HttpTransportCore core;
  core.setStatusBodyProvider(provideStatus, &statusProbe);
  core.setConfigBodyProvider(provideStatus, &configProbe);
  core.beginConnection(0);
  feed(core, "GET /api/config/v1 HTTP/1.1\r\nHost: beacon\r\n\r\n");
  const std::string response = drain(core, 23);
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(0, statusProbe.calls);
  TEST_ASSERT_EQUAL_UINT16(1, configProbe.calls);
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("application/json; charset=utf-8"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("Cache-Control: no-store"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, response.find(configBody));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("Connection: close"));
  assertContentLengthMatches(response);
}

void test_config_provider_failure_is_complete_503_and_post_is_405() {
  StatusProviderProbe configProbe;
  configProbe.succeed = false;
  HttpTransportCore core;
  core.setConfigBodyProvider(provideStatus, &configProbe);
  core.beginConnection(0);
  feed(core, "GET /api/config/v1 HTTP/1.1\r\nHost: beacon\r\n\r\n");
  std::string response = drain(core);
  TEST_ASSERT_EQUAL_UINT16(503, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("\"error\":\"config_unavailable\""));
  assertContentLengthMatches(response);

  core.disconnect();
  core.beginConnection(100);
  feed(core, "POST /api/config/v1 HTTP/1.1\r\n\r\n");
  response = drain(core);
  TEST_ASSERT_EQUAL_UINT16(405, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(1, configProbe.calls);
}

void test_non_status_routes_never_collect_application_snapshot() {
  StatusProviderProbe probe;
  probe.succeed = false;
  HttpTransportCore core;
  core.setStatusBodyProvider(provideStatus, &probe);
  core.beginConnection(0);
  feed(core, "GET / HTTP/1.1\r\nHost: beacon\r\n\r\n");
  drain(core);
  TEST_ASSERT_EQUAL_UINT16(0, probe.calls);
}

void test_csrf_bootstrap_requires_actual_host_and_never_uses_status_route() {
  HttpTransportCore core;
  core.setSecurityContext("192.168.1.76", "0123456789abcdef0123456789abcdef");
  core.beginConnection(0);
  feed(core, "GET /api/config/v1/token HTTP/1.1\r\nHost: evil.test\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(403, core.statusCode());
  drain(core);
  core.disconnect();
  core.beginConnection(100);
  feed(core, "GET /api/config/v1/token HTTP/1.1\r\nHost: 192.168.1.76\r\n\r\n");
  const std::string response = drain(core);
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
      response.find("0123456789abcdef0123456789abcdef"));
}

void test_mutation_requires_same_origin_token_and_complete_incremental_body() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "0123456789abcdef0123456789abcdef");
  const std::string body = "{\"expected_revision\":1}";
  const std::string request =
      "POST /api/config/v1/discard HTTP/1.1\r\nHost: 192.168.1.76\r\n"
      "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: "
      "0123456789abcdef0123456789abcdef\r\nContent-Type: application/json\r\n"
      "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
  core.beginConnection(0);
  feed(core, request.substr(0, request.size() - 3U));
  TEST_ASSERT_EQUAL_UINT16(0, probe.calls);
  feed(core, request.substr(request.size() - 3U), 100);
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_EQUAL_STRING(body.c_str(), probe.body.c_str());
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
}

void test_mutation_security_and_body_limits_fail_without_callback() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "token");
  core.beginConnection(0);
  feed(core, "PUT /api/config/v1/candidate HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Origin: http://other\r\nX-WSPR-CSRF-Token: token\r\n"
             "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
  TEST_ASSERT_EQUAL_UINT16(403, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(0, probe.calls);
  drain(core);
  core.disconnect(); core.beginConnection(100);
  feed(core, "PUT /api/config/v1/candidate HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
             "Content-Type: application/json\r\nContent-Length: 513\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(413, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(0, probe.calls);
}

void test_manual_tx_routes_share_existing_mutation_security() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "token");
  core.beginConnection(0);
  feed(core, "POST /api/tx/v1/off HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
             "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpTransportCore::Mutation::MANUAL_TX_OFF),
      static_cast<uint8_t>(probe.mutation));
  drain(core);
  core.disconnect();
  core.beginConnection(100);
  feed(core, "POST /api/tx/v1/on HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
             "Content-Type: application/json\r\nContent-Length: 19\r\n\r\n"
             "{\"confirm\":\"tx_on\"}");
  TEST_ASSERT_EQUAL_UINT16(2, probe.calls);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpTransportCore::Mutation::MANUAL_TX_ON),
      static_cast<uint8_t>(probe.mutation));
  drain(core);
  core.disconnect();
  core.beginConnection(200);
  feed(core, "POST /api/tx/v1/off HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
  TEST_ASSERT_EQUAL_UINT16(403, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(2, probe.calls);
}

void test_session_get_and_secure_session_mutation_routes() {
  static const char SESSION_BODY[] =
      "{\"schema\":1,\"mode\":\"NORMAL\",\"revision\":0}\n";
  StatusProviderProbe sessionProbe;
  sessionProbe.body = SESSION_BODY;
  sessionProbe.length = sizeof(SESSION_BODY) - 1U;
  MutationProbe mutationProbe;
  HttpTransportCore core;
  core.setSessionBodyProvider(provideStatus, &sessionProbe);
  core.setMutationProvider(performMutation, &mutationProbe);
  core.setSecurityContext("192.168.1.76", "token");
  core.beginConnection(0);
  feed(core, "GET /api/config/v1/session HTTP/1.1\r\nHost: 192.168.1.76\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, drain(core).find("\"mode\":\"NORMAL\""));

  core.disconnect();
  core.beginConnection(100);
  const std::string body =
      "{\"session_id\":\"00112233445566778899aabbccddeeff\","
      "\"expected_revision\":0}";
  const std::string request =
      "POST /api/config/v1/session/enter HTTP/1.1\r\nHost: 192.168.1.76\r\n"
      "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
      "Content-Type: application/json\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body;
  feed(core, request, 100);
  TEST_ASSERT_EQUAL_UINT16(1, mutationProbe.calls);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpTransportCore::Mutation::SESSION_ENTER),
      static_cast<uint8_t>(mutationProbe.mutation));
}

void test_factory_reset_is_protected_post_only_and_profile_isolated() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "token");
  const std::string body =
      "{\"expected_revision\":7,\"confirm\":\"FACTORY RESET\"}";
  const std::string headers =
      " HTTP/1.1\r\nHost: 192.168.1.76\r\nOrigin: http://192.168.1.76\r\n"
      "X-WSPR-CSRF-Token: token\r\nX-WSPR-Session: 00112233445566778899aabbccddeeff\r\n"
      "Content-Type: application/json\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body;
  core.beginConnection(0);
  feed(core, "POST /api/factory/v1/reset" + headers);
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTransportCore::Mutation::FACTORY_RESET),
                          static_cast<uint8_t>(probe.mutation));
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff", probe.session.c_str());
  drain(core);
  core.disconnect(); core.beginConnection(100);
  feed(core, "GET /api/factory/v1/reset HTTP/1.1\r\nHost: 192.168.1.76\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(404, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  core.disconnect(); core.setProfile(HttpTransportCore::Profile::PROVISIONING);
  core.beginConnection(200);
  feed(core, "POST /api/factory/v1/reset" + headers, 200);
  TEST_ASSERT_EQUAL_UINT16(404, core.statusCode());
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
}

void test_candidate_session_header_is_bounded_and_forwarded() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "token");
  const std::string body = "{\"expected_revision\":1}";
  const std::string request =
      "POST /api/config/v1/discard HTTP/1.1\r\nHost: 192.168.1.76\r\n"
      "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
      "X-WSPR-Session: 00112233445566778899aabbccddeeff\r\n"
      "Content-Type: application/json\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body;
  core.beginConnection(0);
  feed(core, request);
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff",
                           probe.session.c_str());
  drain(core);
  core.disconnect();
  core.beginConnection(100);
  feed(core, "POST /api/config/v1/discard HTTP/1.1\r\nHost: 192.168.1.76\r\n"
             "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
             "X-WSPR-Session: 00112233445566778899aabbccddeeff0\r\n"
             "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}");
  TEST_ASSERT_EQUAL_UINT16(403, core.statusCode());
}

void test_secure_save_route_forwards_complete_body_and_owner_session() {
  MutationProbe probe;
  HttpTransportCore core;
  core.setMutationProvider(performMutation, &probe);
  core.setSecurityContext("192.168.1.76", "token");
  const std::string body =
      "{\"expected_revision\":12,\"confirm\":\"save_wspr\"}";
  const std::string request =
      "POST /api/config/v1/save HTTP/1.1\r\nHost: 192.168.1.76\r\n"
      "Origin: http://192.168.1.76\r\nX-WSPR-CSRF-Token: token\r\n"
      "X-WSPR-Session: 00112233445566778899aabbccddeeff\r\n"
      "Content-Type: application/json\r\nContent-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body;
  core.beginConnection(0);
  feed(core, request);
  TEST_ASSERT_EQUAL_UINT16(1, probe.calls);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpTransportCore::Mutation::SAVE_WSPR),
      static_cast<uint8_t>(probe.mutation));
  TEST_ASSERT_EQUAL_STRING(body.c_str(), probe.body.c_str());
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff",
                           probe.session.c_str());
  TEST_ASSERT_EQUAL_UINT16(200, core.statusCode());
  TEST_ASSERT_FALSE(core.complete());
  const std::string response = drain(core);
  TEST_ASSERT_NOT_EQUAL(std::string::npos, response.find("Connection: close"));
  assertContentLengthMatches(response);
}

void test_unsupported_method_is_405_without_body_processing() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "POST / HTTP/1.1\r\n");
  TEST_ASSERT_EQUAL_UINT16(405, core.statusCode());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        drain(core).find("Method Not Allowed"));
}

void test_unknown_path_is_404() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /missing HTTP/1.1\r\nHost: beacon\r\n\r\n");
  TEST_ASSERT_EQUAL_UINT16(404, core.statusCode());
}

void test_oversized_request_line_is_414() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, std::string(HttpTransportCore::MAX_REQUEST_LINE_BYTES + 1, 'G'));
  TEST_ASSERT_EQUAL_UINT16(414, core.statusCode());
}

void test_oversized_headers_are_431() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET / HTTP/1.1\r\n");
  feed(core, std::string(HttpTransportCore::MAX_HEADER_BYTES + 1, 'A'));
  TEST_ASSERT_EQUAL_UINT16(431, core.statusCode());
}

void test_malformed_request_and_header_are_400() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET_ONLY\r\n");
  TEST_ASSERT_EQUAL_UINT16(400, core.statusCode());
  drain(core);
  core.disconnect();
  core.beginConnection(10);
  feed(core, "GET / HTTP/1.1\r\nMalformed\r\n");
  TEST_ASSERT_EQUAL_UINT16(400, core.statusCode());
}

void test_fragmented_request_waits_for_completion() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /api/");
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpTransportState::REQUEST_LINE),
      static_cast<uint8_t>(core.state()));
  feed(core, "status HTTP/1.1\r\nHost: b");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTransportState::HEADERS),
                          static_cast<uint8_t>(core.state()));
  feed(core, "eacon\r\n\r\n");
  TEST_ASSERT_TRUE(core.responseReady());
}

void test_stalled_and_lifetime_timeouts_are_bounded() {
  HttpTransportCore core;
  core.beginConnection(100);
  TEST_ASSERT_FALSE(core.timedOut(299));
  TEST_ASSERT_TRUE(core.timedOut(300));
  core.disconnect();
  core.beginConnection(1000);
  for (uint32_t now = 1100; now < 1600; now += 100) {
    core.consume('G', now);
  }
  TEST_ASSERT_TRUE(core.timedOut(1600));
}

void test_response_deadlines_are_separate_from_request_deadlines() {
  HttpTransportCore json;
  json.beginConnection(100);
  feedAt(json, "GET /api/status HTTP/1.1\r\n\r\n", 100);
  TEST_ASSERT_FALSE(json.timedOut(700));
  size_t available = 0;
  json.outputData(available);
  json.consumeOutput(1, 2500);
  TEST_ASSERT_FALSE(json.timedOut(4100));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::NONE),
                          static_cast<uint8_t>(json.timeoutKind(5099)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::RESPONSE_TOTAL),
                          static_cast<uint8_t>(json.timeoutKind(5100)));

  HttpTransportCore html;
  html.beginConnection(100);
  feedAt(html, "GET / HTTP/1.1\r\n\r\n", 100);
  TEST_ASSERT_FALSE(html.timedOut(700));
  html.outputData(available);
  html.consumeOutput(1, 3000);
  html.consumeOutput(1, 5500);
  html.consumeOutput(1, 8000);
  html.consumeOutput(1, 10500);
  html.consumeOutput(1, 13000);
  TEST_ASSERT_FALSE(html.timedOut(14999));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::RESPONSE_TOTAL),
                          static_cast<uint8_t>(html.timeoutKind(15100)));
}

void test_response_stall_is_bounded_only_by_absolute_deadline() {
  HttpTransportCore stalled;
  stalled.beginConnection(100);
  feedAt(stalled, "GET /api/status HTTP/1.1\r\n\r\n", 100);
  TEST_ASSERT_FALSE(stalled.timedOut(3100));
  TEST_ASSERT_FALSE(stalled.timedOut(5099));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::RESPONSE_TOTAL),
                          static_cast<uint8_t>(stalled.timeoutKind(5100)));
}

void test_response_can_resume_and_complete_after_long_stall() {
  HttpTransportCore core;
  core.beginConnection(100);
  feedAt(core, "GET / HTTP/1.1\r\n\r\n", 100);
  TEST_ASSERT_FALSE(core.timedOut(5100));
  size_t available = 0;
  size_t submitted = 0;
  while (core.responseReady()) {
    TEST_ASSERT_NOT_NULL(core.outputData(available));
    const size_t count = available < 536U ? available : 536U;
    submitted += count;
    core.consumeOutput(count, 5200);
  }
  TEST_ASSERT_TRUE(core.complete());
  TEST_ASSERT_TRUE(submitted > dashboardPageSize());
}

void test_zero_byte_write_keeps_absolute_response_deadline() {
  HttpTransportCore core;
  core.beginConnection(50);
  feedAt(core, "GET /api/status HTTP/1.1\r\n\r\n", 50);
  core.consumeOutput(0, 2900);
  TEST_ASSERT_FALSE(core.timedOut(3050));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::RESPONSE_TOTAL),
                          static_cast<uint8_t>(core.timeoutKind(5050)));
}

void test_complete_dashboard_response_under_bounded_backpressure() {
  HttpTransportCore core;
  core.beginConnection(10);
  feedAt(core, "GET / HTTP/1.1\r\n\r\n", 10);
  size_t submitted = 0;
  uint32_t now = 30;
  while (core.responseReady()) {
    TEST_ASSERT_FALSE(core.timedOut(now));
    size_t available = 0;
    TEST_ASSERT_NOT_NULL(core.outputData(available));
    const size_t count = available < 128U ? available : 128U;
    submitted += count;
    core.consumeOutput(count, now);
    now += 20;
  }
  TEST_ASSERT_TRUE(core.complete());
  TEST_ASSERT_TRUE(submitted > dashboardPageSize());
  TEST_ASSERT_TRUE(now > HttpTransportCore::CLIENT_LIFETIME_MS);
}

void test_complete_maximum_json_response_under_backpressure() {
  static char body[2048];
  memset(body, 'J', sizeof(body));
  StatusProviderProbe probe;
  probe.body = body;
  probe.length = sizeof(body);
  HttpTransportCore core;
  core.setStatusBodyProvider(provideStatus, &probe);
  core.beginConnection(10);
  feedAt(core, "GET /api/status HTTP/1.1\r\n\r\n", 10);
  uint32_t now = 90;
  while (core.responseReady()) {
    TEST_ASSERT_FALSE(core.timedOut(now));
    size_t available = 0;
    TEST_ASSERT_NOT_NULL(core.outputData(available));
    const size_t count = available < 128U ? available : 128U;
    core.consumeOutput(count, now);
    now += 80;
  }
  TEST_ASSERT_TRUE(core.complete());
  TEST_ASSERT_TRUE(now > HttpTransportCore::CLIENT_LIFETIME_MS);
}

void test_esp8266_write_plan_honors_capacity_and_chunk_limit() {
  TEST_ASSERT_EQUAL_UINT32(
      0, HttpWritePlanner::plannedBytes(1000, 0, true));
  TEST_ASSERT_EQUAL_UINT32(
      1, HttpWritePlanner::plannedBytes(1000, 1, true));
  TEST_ASSERT_EQUAL_UINT32(
      535, HttpWritePlanner::plannedBytes(1000, 535, true));
  TEST_ASSERT_EQUAL_UINT32(
      HttpWritePlanner::MAX_CHUNK_BYTES,
      HttpWritePlanner::plannedBytes(1000, 536, true));
  TEST_ASSERT_EQUAL_UINT32(
      HttpWritePlanner::MAX_CHUNK_BYTES,
      HttpWritePlanner::plannedBytes(1000, 2048, true));
  TEST_ASSERT_EQUAL_UINT32(
      91, HttpWritePlanner::plannedBytes(91, 2048, true));
}

void test_esp32_write_plan_does_not_treat_zero_as_backpressure() {
  TEST_ASSERT_EQUAL_UINT32(
      HttpWritePlanner::MAX_CHUNK_BYTES,
      HttpWritePlanner::plannedBytes(1000, 0, false));
  TEST_ASSERT_EQUAL_UINT32(
      73, HttpWritePlanner::plannedBytes(73, 0, false));
}

void test_nonblocking_writer_submits_once_and_reports_full_or_partial_progress() {
  static const uint8_t DATA[] = "abcdef";
  SocketSendProbe full;
  full.results = {6};
  HttpSocketWriteResult result = HttpNonBlockingWriter::attempt(
      12, DATA, 6, simulatedSend, &full);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpSocketWriteStatus::PROGRESS),
                          static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(6, result.bytesWritten);
  TEST_ASSERT_EQUAL_UINT32(1, full.calls);
  TEST_ASSERT_EQUAL_INT(12, full.lastDescriptor);
  TEST_ASSERT_EQUAL_STRING("abcdef", full.submitted.c_str());

  SocketSendProbe partial;
  partial.results = {2};
  result = HttpNonBlockingWriter::attempt(13, DATA, 6, simulatedSend, &partial);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpSocketWriteStatus::PROGRESS),
                          static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(2, result.bytesWritten);
  TEST_ASSERT_EQUAL_UINT32(1, partial.calls);
  TEST_ASSERT_EQUAL_STRING("ab", partial.submitted.c_str());
}

void test_nonblocking_writer_defers_temporary_socket_conditions_without_retry() {
  static const uint8_t DATA[] = "abc";
  const int temporaryErrors[] = {EAGAIN, EWOULDBLOCK, EINTR};
  const HttpSocketWriteStatus expected[] = {
      HttpSocketWriteStatus::WOULD_BLOCK,
      HttpSocketWriteStatus::WOULD_BLOCK,
      HttpSocketWriteStatus::INTERRUPTED};
  for (size_t index = 0; index < 3; ++index) {
    SocketSendProbe probe;
    probe.results = {-1};
    probe.errors = {temporaryErrors[index]};
    const HttpSocketWriteResult result = HttpNonBlockingWriter::attempt(
        7, DATA, 3, simulatedSend, &probe);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected[index]),
                            static_cast<uint8_t>(result.status));
    TEST_ASSERT_EQUAL_UINT32(0, result.bytesWritten);
    TEST_ASSERT_EQUAL_UINT32(1, probe.calls);
    TEST_ASSERT_TRUE(probe.submitted.empty());
  }
}

void test_nonblocking_writer_distinguishes_closed_and_definitive_error() {
  static const uint8_t DATA[] = "abc";
  SocketSendProbe closed;
  closed.results = {0};
  HttpSocketWriteResult result = HttpNonBlockingWriter::attempt(
      7, DATA, 3, simulatedSend, &closed);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpSocketWriteStatus::DISCONNECTED),
      static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(1, closed.calls);

  SocketSendProbe failed;
  failed.results = {-1};
  failed.errors = {ECONNRESET};
  result = HttpNonBlockingWriter::attempt(7, DATA, 3, simulatedSend, &failed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpSocketWriteStatus::ERROR),
                          static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(1, failed.calls);

  SocketSendProbe invalid;
  result = HttpNonBlockingWriter::attempt(-1, DATA, 3, simulatedSend, &invalid);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpSocketWriteStatus::DISCONNECTED),
      static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(0, invalid.calls);
}

void test_nonblocking_writer_backpressure_preserves_exact_response_sequence() {
  HttpTransportCore expected;
  expected.beginConnection(0);
  feedAt(expected, "GET / HTTP/1.1\r\nAccept-Encoding: gzip\r\n\r\n", 0);
  const std::string complete = drain(expected);

  HttpTransportCore core;
  core.beginConnection(0);
  feedAt(core, "GET / HTTP/1.1\r\nAccept-Encoding: gzip\r\n\r\n", 0);
  SocketSendProbe probe;
  uint32_t now = 1;
  size_t turn = 0;
  while (core.responseReady()) {
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    const size_t planned = HttpWritePlanner::plannedBytes(available, 0, false);
    if (turn % 5U == 0U) {
      probe.results.push_back(-1);
      probe.errors.push_back(EAGAIN);
    } else if (turn % 5U == 1U) {
      probe.results.push_back(-1);
      probe.errors.push_back(EINTR);
    } else {
      const size_t accepted = planned > 17U ? 17U : planned;
      probe.results.push_back(static_cast<int>(accepted));
      probe.errors.push_back(0);
    }
    const size_t callsBefore = probe.calls;
    const HttpSocketWriteResult result = HttpNonBlockingWriter::attempt(
        9, data, planned, simulatedSend, &probe);
    TEST_ASSERT_EQUAL_UINT32(callsBefore + 1U, probe.calls);
    core.consumeOutput(result.bytesWritten, now++);
    ++turn;
  }
  TEST_ASSERT_EQUAL_UINT32(complete.size(), probe.submitted.size());
  TEST_ASSERT_EQUAL_STRING_LEN(complete.c_str(), probe.submitted.c_str(),
                               complete.size());
}

void test_partial_writes_preserve_response_byte_sequence() {
  HttpTransportCore expected;
  expected.beginConnection(0);
  feedAt(expected, "GET / HTTP/1.1\r\n\r\n", 0);
  const std::string complete = drain(expected);

  HttpTransportCore core;
  core.beginConnection(0);
  feedAt(core, "GET / HTTP/1.1\r\n\r\n", 0);
  std::string submitted;
  uint32_t now = 10;
  size_t iteration = 0;
  while (core.responseReady()) {
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    TEST_ASSERT_NOT_NULL(data);
    const size_t planned = HttpWritePlanner::plannedBytes(available, 536, true);
    TEST_ASSERT_TRUE(planned > 0);
    const size_t accepted = (iteration++ % 3U == 0U && planned > 1U)
                                ? planned / 2U
                                : planned;
    submitted.append(reinterpret_cast<const char *>(data), accepted);
    core.consumeOutput(accepted, now++);
  }
  TEST_ASSERT_EQUAL_STRING_LEN(complete.c_str(), submitted.c_str(),
                               complete.size());
  TEST_ASSERT_EQUAL_UINT32(complete.size(), submitted.size());
}

void test_zero_capacity_recovers_without_advancing_response() {
  HttpTransportCore core;
  core.beginConnection(0);
  feedAt(core, "GET /api/status HTTP/1.1\r\n\r\n", 0);
  size_t before = 0;
  const uint8_t *initial = core.outputData(before);
  TEST_ASSERT_NOT_NULL(initial);
  TEST_ASSERT_EQUAL_UINT32(
      0, HttpWritePlanner::plannedBytes(before, 0, true));
  core.consumeOutput(0, 100);
  size_t after = 0;
  TEST_ASSERT_EQUAL_PTR(initial, core.outputData(after));
  TEST_ASSERT_EQUAL_UINT32(before, after);
  TEST_ASSERT_TRUE(HttpWritePlanner::plannedBytes(after, 536, true) > 0);
}

void test_variable_partial_capacity_completes_flash_response() {
  HttpTransportCore expected;
  expected.beginConnection(0);
  feedAt(expected, "GET / HTTP/1.1\r\nAccept-Encoding: gzip\r\n\r\n", 0);
  const std::string complete = drain(expected);

  HttpTransportCore core;
  core.beginConnection(0);
  feedAt(core, "GET / HTTP/1.1\r\nAccept-Encoding: gzip\r\n\r\n", 0);
  const int capacities[] = {0, 1, 127, 535, 536, 2048};
  std::string submitted;
  uint32_t now = 10;
  size_t iteration = 0;
  while (core.responseReady()) {
    TEST_ASSERT_FALSE(core.timedOut(now));
    size_t available = 0;
    const uint8_t *data = core.outputData(available);
    TEST_ASSERT_NOT_NULL(data);
    const int capacity = capacities[iteration++ %
                                    (sizeof(capacities) / sizeof(capacities[0]))];
    const size_t planned =
        HttpWritePlanner::plannedBytes(available, capacity, true);
    TEST_ASSERT_TRUE(planned <= available);
    TEST_ASSERT_TRUE(planned <= HttpWritePlanner::MAX_CHUNK_BYTES);
    TEST_ASSERT_TRUE(capacity <= 0 || planned <= static_cast<size_t>(capacity));
    submitted.append(reinterpret_cast<const char *>(data), planned);
    core.consumeOutput(planned, now);
    now += 25;
  }
  TEST_ASSERT_EQUAL_UINT32(complete.size(), submitted.size());
  TEST_ASSERT_EQUAL_STRING_LEN(complete.c_str(), submitted.c_str(),
                               complete.size());
}

void test_response_output_is_incremental() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\n\r\n");
  const std::string response = drain(core, 7);
  TEST_ASSERT_TRUE(core.complete());
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        response.find("Content-Length:"));
}

void test_final_write_completes_response_submission() {
  HttpTransportCore core;
  core.beginConnection(0);
  feed(core, "GET /api/status HTTP/1.1\r\n\r\n");
  uint32_t now = 100;
  while (core.responseReady()) {
    size_t available = 0;
    core.outputData(available);
    const size_t partial = available > 5 ? 5 : available;
    core.consumeOutput(partial, now++);
  }
  TEST_ASSERT_TRUE(core.complete());
}

void test_single_client_is_enforced() {
  TEST_ASSERT_EQUAL_UINT8(1, HttpTransportCore::LISTENER_BACKLOG);
  HttpTransportCore core;
  TEST_ASSERT_TRUE(core.beginConnection(0));
  TEST_ASSERT_FALSE(core.beginConnection(1));
  core.disconnect();
  TEST_ASSERT_TRUE(core.beginConnection(2));
}

void test_wifi_disconnect_stops_listener_and_closes_client() {
  HttpServiceLifecycle lifecycle;
  HttpLifecycleActions actions = lifecycle.update(true, false);
  TEST_ASSERT_TRUE(actions.startListener);
  lifecycle.listenerStarted();
  actions = lifecycle.update(false, true);
  TEST_ASSERT_TRUE(actions.stopListener);
  TEST_ASSERT_TRUE(actions.closeClient);
  TEST_ASSERT_FALSE(actions.serviceClient);
}

void test_listener_restarts_when_service_returns() {
  HttpServiceLifecycle lifecycle;
  lifecycle.update(true, false, 0);
  lifecycle.listenerStarted();
  lifecycle.update(false, false);
  const HttpLifecycleActions actions = lifecycle.update(true, false, 1);
  TEST_ASSERT_TRUE(actions.startListener);
  TEST_ASSERT_FALSE(actions.serviceClient);
}

void test_listener_runs_after_framework_start() {
  HttpServiceLifecycle lifecycle;
  TEST_ASSERT_TRUE(lifecycle.update(true, false, 100).startListener);
  lifecycle.listenerStarted();
  const HttpLifecycleActions actions = lifecycle.update(true, false, 101);
  TEST_ASSERT_FALSE(actions.startListener);
  TEST_ASSERT_TRUE(actions.serviceClient);
}

void test_t_minus_seven_guard_is_fail_closed() {
  HttpServiceGateInputs inputs = safeGate();
  TEST_ASSERT_TRUE(HttpServiceGate::allowed(inputs));
  inputs.millisUntilSlot = HttpServiceGate::SLOT_GUARD_MS;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_configuration_keeps_http_open_across_nominal_slot_guard() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.configurationActive = true;
  inputs.millisUntilSlot = 0;
  TEST_ASSERT_TRUE(HttpServiceGate::allowed(inputs));

  inputs.rfPhase = HttpRfPhase::PREPARING;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
  inputs.rfPhase = HttpRfPhase::TRANSMITTING;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
  inputs.rfPhase = HttpRfPhase::SAFE;
  inputs.wifiConnected = false;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_resume_guard_does_not_bypass_normal_http_policy() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.configurationActive = false;
  inputs.millisUntilSlot = HttpServiceGate::SLOT_GUARD_MS;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_preparing_tx_and_active_client_are_excluded() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.rfPhase = HttpRfPhase::PREPARING;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
  inputs.rfPhase = HttpRfPhase::TRANSMITTING;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));

  HttpServiceLifecycle lifecycle;
  lifecycle.update(true, false);
  const HttpLifecycleActions actions = lifecycle.update(false, true);
  TEST_ASSERT_TRUE(actions.closeClient);
}

void test_gate_interrupts_request_and_response_without_waiting_for_deadlines() {
  HttpTransportCore request;
  request.beginConnection(0);
  feedAt(request, "GET /api", 0);
  TEST_ASSERT_FALSE(request.responseReady());
  HttpServiceLifecycle requestLifecycle;
  requestLifecycle.update(true, false);
  requestLifecycle.listenerStarted();
  HttpLifecycleActions actions = requestLifecycle.update(false, request.active());
  TEST_ASSERT_TRUE(actions.closeClient);
  TEST_ASSERT_FALSE(actions.serviceClient);

  HttpTransportCore response;
  response.beginConnection(0);
  feedAt(response, "GET / HTTP/1.1\r\n\r\n", 0);
  TEST_ASSERT_TRUE(response.responseReady());
  HttpServiceLifecycle responseLifecycle;
  responseLifecycle.update(true, false);
  responseLifecycle.listenerStarted();
  actions = responseLifecycle.update(false, response.active());
  TEST_ASSERT_TRUE(actions.closeClient);
  TEST_ASSERT_FALSE(actions.serviceClient);
}

void test_late_qualification_closes_http_immediately() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.authorization = WsprTimeAuthorization::NONE;
  inputs.nextSlotKnown = false;
  TEST_ASSERT_TRUE(HttpServiceGate::allowed(inputs));
  inputs.authorization = WsprTimeAuthorization::NTP_EXPERIMENTAL;
  inputs.nextSlotKnown = true;
  inputs.millisUntilSlot = 6000;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_unknown_slot_with_authority_is_fail_closed() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.nextSlotKnown = false;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_rf_abort_and_missed_slot_return_to_safe_service() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.rfOperational = false;
  inputs.nextSlotKnown = false;
  TEST_ASSERT_TRUE(HttpServiceGate::allowed(inputs));

  inputs.rfOperational = true;
  inputs.millisUntilSlot = 4UL * 60000UL - 1UL;
  inputs.nextSlotKnown = true;
  TEST_ASSERT_TRUE(HttpServiceGate::allowed(inputs));
}

void test_scheduler_supplies_guard_margin() {
  uint32_t remaining = 0;
  TEST_ASSERT_TRUE(WsprScheduler::millisUntilNextSlot(
      validTime(233000UL), 4, remaining));
  TEST_ASSERT_EQUAL_UINT32(7000UL, remaining);
  TEST_ASSERT_FALSE(WsprScheduler::millisUntilNextSlot(
      TimeSnapshot(), 4, remaining));
}

void test_millis_overflow_preserves_timeouts() {
  HttpTransportCore core;
  core.beginConnection(UINT32_MAX - 100UL);
  TEST_ASSERT_FALSE(core.timedOut(98UL));
  TEST_ASSERT_TRUE(core.timedOut(99UL));
}

void test_millis_overflow_preserves_absolute_response_timeout() {
  HttpTransportCore core;
  core.beginConnection(UINT32_MAX - 100UL);
  feedAt(core, "GET /api/status HTTP/1.1\r\n\r\n", UINT32_MAX - 100UL);
  TEST_ASSERT_FALSE(core.timedOut(4898UL));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpTimeoutKind::RESPONSE_TOTAL),
                          static_cast<uint8_t>(core.timeoutKind(4899UL)));
}

void test_no_wifi_means_no_http_activity() {
  HttpServiceGateInputs inputs = safeGate();
  inputs.wifiConnected = false;
  TEST_ASSERT_FALSE(HttpServiceGate::allowed(inputs));
}

void test_gate_decisions_report_existing_closure_reasons() {
  HttpServiceGateInputs inputs = safeGate();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpGateCloseReason::NONE),
      static_cast<uint8_t>(HttpServiceGate::evaluate(inputs).reason));
  inputs.wifiConnected = false;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpGateCloseReason::WIFI_DISCONNECTED),
      static_cast<uint8_t>(HttpServiceGate::evaluate(inputs).reason));
  inputs = safeGate();
  inputs.rfPhase = HttpRfPhase::PREPARING;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpGateCloseReason::RF_CRITICAL),
      static_cast<uint8_t>(HttpServiceGate::evaluate(inputs).reason));
  inputs = safeGate();
  inputs.millisUntilSlot = HttpServiceGate::SLOT_GUARD_MS;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(HttpGateCloseReason::SLOT_GUARD),
      static_cast<uint8_t>(HttpServiceGate::evaluate(inputs).reason));
  inputs = safeGate();
  inputs.nextSlotKnown = false;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(HttpGateCloseReason::TIME_UNCERTAIN),
      static_cast<uint8_t>(HttpServiceGate::evaluate(inputs).reason));
}

void test_http_diagnostics_track_gate_listener_and_responses() {
  HttpDiagnostics diagnostics;
  diagnostics.observeGate(false, HttpGateCloseReason::WIFI_DISCONNECTED, 10);
  diagnostics.observeGate(false, HttpGateCloseReason::WIFI_DISCONNECTED, 11);
  diagnostics.observeGate(true, HttpGateCloseReason::NONE, 20);
  diagnostics.listenerStarted();
  diagnostics.clientAccepted();
  diagnostics.responseServed(200);
  const HttpDiagnosticsSnapshot &snapshot = diagnostics.snapshot();
  TEST_ASSERT_TRUE(snapshot.gateOpen());
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.gateOpenings);
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.gateClosures);
  TEST_ASSERT_EQUAL_UINT32(20, snapshot.lastGateTransitionMillis);
  TEST_ASSERT_TRUE(snapshot.gateTransitionValid());
  uint32_t gateAgeMillis = 0;
  TEST_ASSERT_TRUE(snapshot.gateAgeMillis(20, gateAgeMillis));
  TEST_ASSERT_EQUAL_UINT32(0, gateAgeMillis);
  TEST_ASSERT_TRUE(snapshot.listenerIntended());
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.clientsAccepted);
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.responsesServed);

  diagnostics.listenerStopped();
  TEST_ASSERT_FALSE(diagnostics.snapshot().listenerIntended());
}

void test_http_gate_age_tracks_post_tx_opening_and_progression() {
  HttpDiagnostics diagnostics;
  diagnostics.observeGate(false, HttpGateCloseReason::RF_CRITICAL, 1000);
  diagnostics.observeGate(true, HttpGateCloseReason::NONE, 2000);
  uint32_t ageMillis = UINT32_MAX;
  TEST_ASSERT_TRUE(diagnostics.snapshot().gateAgeMillis(2000, ageMillis));
  TEST_ASSERT_EQUAL_UINT32(0, ageMillis);
  TEST_ASSERT_TRUE(diagnostics.snapshot().gateAgeMillis(2125, ageMillis));
  TEST_ASSERT_EQUAL_UINT32(125, ageMillis);
}

void test_http_gate_age_distinguishes_zero_invalid_future_and_wrap() {
  HttpDiagnostics diagnostics;
  uint32_t ageMillis = UINT32_MAX;
  TEST_ASSERT_FALSE(diagnostics.snapshot().gateAgeMillis(0, ageMillis));

  diagnostics.observeGate(true, HttpGateCloseReason::NONE, 0);
  TEST_ASSERT_TRUE(diagnostics.snapshot().gateTransitionValid());
  TEST_ASSERT_TRUE(diagnostics.snapshot().gateAgeMillis(0, ageMillis));
  TEST_ASSERT_EQUAL_UINT32(0, ageMillis);

  diagnostics.observeGate(false, HttpGateCloseReason::SLOT_GUARD, 101);
  TEST_ASSERT_FALSE(diagnostics.snapshot().gateAgeMillis(100, ageMillis));

  diagnostics.observeGate(true, HttpGateCloseReason::NONE,
                          UINT32_MAX - 49UL);
  TEST_ASSERT_TRUE(diagnostics.snapshot().gateAgeMillis(50, ageMillis));
  TEST_ASSERT_EQUAL_UINT32(100, ageMillis);
}

void test_http_diagnostics_track_failures_and_forced_closures() {
  HttpDiagnostics diagnostics;
  diagnostics.responseServed(404);
  diagnostics.clientTimedOut();
  diagnostics.clientTimedOut(true);
  diagnostics.requestFailed();
  diagnostics.forcedClosure();
  const HttpDiagnosticsSnapshot &snapshot = diagnostics.snapshot();
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.responsesServed);
  TEST_ASSERT_EQUAL_UINT16(2, snapshot.clientTimeouts);
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.requestTimeouts);
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.responseTimeouts);
  TEST_ASSERT_EQUAL_UINT16(4, snapshot.failedRequests);
  TEST_ASSERT_EQUAL_UINT16(1, snapshot.forcedClosures);
}

void test_http_diagnostic_counters_saturate() {
  HttpDiagnostics diagnostics;
  for (uint32_t index = 0; index <= UINT16_MAX; ++index) {
    diagnostics.clientAccepted();
  }
  diagnostics.clientAccepted();
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX,
                           diagnostics.snapshot().clientsAccepted);
}

void test_http_transfer_metrics_cover_complete_partial_zero_and_capacity() {
  HttpDiagnostics diagnostics;
  diagnostics.clientAccepted();
  diagnostics.recordWrite(536, 536, 3);
  diagnostics.recordWrite(536, 200, 17);
  diagnostics.recordWrite(536, 0, 8);
  diagnostics.noCapacityTurn();
  diagnostics.responseServed(200, 42, 736);
  const HttpDiagnosticsSnapshot &s = diagnostics.snapshot();
  TEST_ASSERT_EQUAL_UINT16(1, s.clientsAccepted);
  TEST_ASSERT_EQUAL_UINT16(1, s.responsesServed);
  TEST_ASSERT_EQUAL_UINT32(736, s.bytesSent);
  TEST_ASSERT_EQUAL_UINT16(1, s.partialWrites);
  TEST_ASSERT_EQUAL_UINT16(1, s.zeroWrites);
  TEST_ASSERT_EQUAL_UINT16(1, s.noCapacityTurns);
  TEST_ASSERT_EQUAL_UINT16(17, s.maxWriteMillis);
  TEST_ASSERT_EQUAL_UINT16(42, s.maxResponseMillis);
  TEST_ASSERT_EQUAL_UINT16(42, s.lastResponseMillis);
  TEST_ASSERT_EQUAL_UINT16(736, s.lastResponseBytes);
  TEST_ASSERT_TRUE(s.lastResponseComplete());
}

void test_http_transfer_metrics_distinguish_gate_timeout_disconnect() {
  HttpDiagnostics diagnostics;
  diagnostics.responseInterrupted(300, 150);
  diagnostics.clientClosed(HttpClientCloseReason::GATE);
  diagnostics.clientTimedOut(true);
  diagnostics.responseInterrupted(900, 210);
  diagnostics.clientClosed(HttpClientCloseReason::TIMEOUT);
  diagnostics.requestFailed();
  diagnostics.responseInterrupted(500, 80);
  diagnostics.clientClosed(HttpClientCloseReason::DISCONNECT);
  const HttpDiagnosticsSnapshot &s = diagnostics.snapshot();
  TEST_ASSERT_EQUAL_UINT16(3, s.responsesInterrupted);
  TEST_ASSERT_EQUAL_UINT16(1, s.gateClientClosures);
  TEST_ASSERT_EQUAL_UINT16(1, s.clientTimeouts);
  TEST_ASSERT_EQUAL_UINT16(1, s.responseTimeouts);
  TEST_ASSERT_EQUAL_UINT16(1, s.disconnectClosures);
  TEST_ASSERT_EQUAL_UINT16(900, s.maxResponseMillis);
  TEST_ASSERT_EQUAL_UINT16(500, s.lastResponseMillis);
  TEST_ASSERT_EQUAL_UINT16(80, s.lastResponseBytes);
  TEST_ASSERT_FALSE(s.lastResponseComplete());
}

void test_http_transfer_metrics_saturate_and_preserve_maxima() {
  HttpDiagnostics diagnostics;
  diagnostics.recordWrite(UINT32_MAX, UINT32_MAX, UINT32_MAX);
  diagnostics.recordWrite(1, 1, 1);
  diagnostics.responseServed(200, UINT32_MAX, UINT32_MAX);
  for (uint32_t index = 0; index <= UINT16_MAX; ++index) {
    diagnostics.noCapacityTurn();
    diagnostics.clientClosed(HttpClientCloseReason::GATE);
  }
  const HttpDiagnosticsSnapshot &s = diagnostics.snapshot();
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, s.bytesSent);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, s.maxWriteMillis);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, s.maxResponseMillis);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, s.lastResponseBytes);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, s.noCapacityTurns);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, s.gateClientClosures);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_root_get);
  RUN_TEST(test_root_header_is_ram_and_dashboard_body_is_program_memory);
  RUN_TEST(test_valid_configuration_page_get_uses_program_memory);
  RUN_TEST(test_static_pages_negotiate_deterministic_gzip_assets);
  RUN_TEST(test_static_pages_preserve_identity_fallback);
  RUN_TEST(test_json_responses_are_never_gzip_encoded);
  RUN_TEST(test_valid_status_get);
  RUN_TEST(test_status_provider_is_on_demand_and_response_is_bounded);
  RUN_TEST(test_status_provider_failure_returns_complete_503_json);
  RUN_TEST(test_valid_config_get_uses_its_own_read_only_provider);
  RUN_TEST(test_config_provider_failure_is_complete_503_and_post_is_405);
  RUN_TEST(test_non_status_routes_never_collect_application_snapshot);
  RUN_TEST(test_csrf_bootstrap_requires_actual_host_and_never_uses_status_route);
  RUN_TEST(test_mutation_requires_same_origin_token_and_complete_incremental_body);
  RUN_TEST(test_mutation_security_and_body_limits_fail_without_callback);
  RUN_TEST(test_manual_tx_routes_share_existing_mutation_security);
  RUN_TEST(test_factory_reset_is_protected_post_only_and_profile_isolated);
  RUN_TEST(test_session_get_and_secure_session_mutation_routes);
  RUN_TEST(test_candidate_session_header_is_bounded_and_forwarded);
  RUN_TEST(test_secure_save_route_forwards_complete_body_and_owner_session);
  RUN_TEST(test_unsupported_method_is_405_without_body_processing);
  RUN_TEST(test_unknown_path_is_404);
  RUN_TEST(test_oversized_request_line_is_414);
  RUN_TEST(test_oversized_headers_are_431);
  RUN_TEST(test_malformed_request_and_header_are_400);
  RUN_TEST(test_fragmented_request_waits_for_completion);
  RUN_TEST(test_stalled_and_lifetime_timeouts_are_bounded);
  RUN_TEST(test_response_deadlines_are_separate_from_request_deadlines);
  RUN_TEST(test_response_stall_is_bounded_only_by_absolute_deadline);
  RUN_TEST(test_response_can_resume_and_complete_after_long_stall);
  RUN_TEST(test_zero_byte_write_keeps_absolute_response_deadline);
  RUN_TEST(test_complete_dashboard_response_under_bounded_backpressure);
  RUN_TEST(test_complete_maximum_json_response_under_backpressure);
  RUN_TEST(test_esp8266_write_plan_honors_capacity_and_chunk_limit);
  RUN_TEST(test_esp32_write_plan_does_not_treat_zero_as_backpressure);
  RUN_TEST(test_nonblocking_writer_submits_once_and_reports_full_or_partial_progress);
  RUN_TEST(test_nonblocking_writer_defers_temporary_socket_conditions_without_retry);
  RUN_TEST(test_nonblocking_writer_distinguishes_closed_and_definitive_error);
  RUN_TEST(test_nonblocking_writer_backpressure_preserves_exact_response_sequence);
  RUN_TEST(test_partial_writes_preserve_response_byte_sequence);
  RUN_TEST(test_zero_capacity_recovers_without_advancing_response);
  RUN_TEST(test_variable_partial_capacity_completes_flash_response);
  RUN_TEST(test_response_output_is_incremental);
  RUN_TEST(test_final_write_completes_response_submission);
  RUN_TEST(test_single_client_is_enforced);
  RUN_TEST(test_wifi_disconnect_stops_listener_and_closes_client);
  RUN_TEST(test_listener_restarts_when_service_returns);
  RUN_TEST(test_listener_runs_after_framework_start);
  RUN_TEST(test_t_minus_seven_guard_is_fail_closed);
  RUN_TEST(test_configuration_keeps_http_open_across_nominal_slot_guard);
  RUN_TEST(test_resume_guard_does_not_bypass_normal_http_policy);
  RUN_TEST(test_preparing_tx_and_active_client_are_excluded);
  RUN_TEST(test_gate_interrupts_request_and_response_without_waiting_for_deadlines);
  RUN_TEST(test_late_qualification_closes_http_immediately);
  RUN_TEST(test_unknown_slot_with_authority_is_fail_closed);
  RUN_TEST(test_rf_abort_and_missed_slot_return_to_safe_service);
  RUN_TEST(test_scheduler_supplies_guard_margin);
  RUN_TEST(test_millis_overflow_preserves_timeouts);
  RUN_TEST(test_millis_overflow_preserves_absolute_response_timeout);
  RUN_TEST(test_no_wifi_means_no_http_activity);
  RUN_TEST(test_gate_decisions_report_existing_closure_reasons);
  RUN_TEST(test_http_diagnostics_track_gate_listener_and_responses);
  RUN_TEST(test_http_gate_age_tracks_post_tx_opening_and_progression);
  RUN_TEST(test_http_gate_age_distinguishes_zero_invalid_future_and_wrap);
  RUN_TEST(test_http_diagnostics_track_failures_and_forced_closures);
  RUN_TEST(test_http_diagnostic_counters_saturate);
  RUN_TEST(test_http_transfer_metrics_cover_complete_partial_zero_and_capacity);
  RUN_TEST(test_http_transfer_metrics_distinguish_gate_timeout_disconnect);
  RUN_TEST(test_http_transfer_metrics_saturate_and_preserve_maxima);
  return UNITY_END();
}
