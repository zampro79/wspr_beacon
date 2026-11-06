#include "HttpTransportCore.h"
#include "DashboardPage.h"
#include "ConfigurationPage.h"
#include "ProvisioningPage.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

namespace {
const char STATUS_BODY[] =
    "{\"schema\":1,\"service\":\"wspr-beacon\",\"status\":\"available\"}\n";
const char STATUS_UNAVAILABLE_BODY[] =
    "{\"schema\":1,\"error\":\"status_unavailable\"}\n";
const char CONFIG_UNAVAILABLE_BODY[] =
    "{\"schema\":1,\"error\":\"config_unavailable\"}\n";
const char SESSION_UNAVAILABLE_BODY[] =
    "{\"schema\":1,\"error\":\"session_unavailable\"}\n";
const char BAD_REQUEST_BODY[] = "Bad Request\n";
const char BAD_REQUEST_JSON[] = "{\"schema\":1,\"error\":\"bad_request\"}\n";
const char NOT_FOUND_BODY[] = "Not Found\n";
const char METHOD_NOT_ALLOWED_BODY[] = "Method Not Allowed\n";
const char REQUEST_LINE_TOO_LONG_BODY[] = "Request Line Too Long\n";
const char HEADERS_TOO_LARGE_BODY[] = "Request Headers Too Large\n";
const char FORBIDDEN_BODY[] = "{\"schema\":1,\"error\":\"forbidden\"}\n";
const char LENGTH_REQUIRED_BODY[] = "{\"schema\":1,\"error\":\"content_length_required\"}\n";
const char BODY_TOO_LARGE[] = "{\"schema\":1,\"error\":\"body_too_large\"}\n";
const char UNSUPPORTED_MEDIA_BODY[] = "{\"schema\":1,\"error\":\"unsupported_content_type\"}\n";
const char MUTATION_UNAVAILABLE_BODY[] = "{\"schema\":1,\"error\":\"mutation_unavailable\"}\n";

bool equalIgnoreCase(const char *left, const char *right) {
  while (*left && *right) {
    if (tolower(static_cast<unsigned char>(*left++)) !=
        tolower(static_cast<unsigned char>(*right++))) return false;
  }
  return *left == *right;
}

bool constantTimeEqual(const char *left, const char *right) {
  const size_t leftLength = strlen(left), rightLength = strlen(right);
  unsigned difference = static_cast<unsigned>(leftLength ^ rightLength);
  const size_t length = leftLength > rightLength ? leftLength : rightLength;
  for (size_t index = 0; index < length; ++index) {
    const unsigned l = index < leftLength ? static_cast<unsigned char>(left[index]) : 0U;
    const unsigned r = index < rightLength ? static_cast<unsigned char>(right[index]) : 0U;
    difference |= l ^ r;
  }
  return difference == 0U;
}

bool acceptsGzip(const char *value) {
  while (*value != '\0') {
    while (*value == ' ' || *value == '\t' || *value == ',') ++value;
    const char *tokenStart = value;
    while (*value != '\0' && *value != ',' && *value != ';') ++value;
    const char *tokenEnd = value;
    while (tokenEnd > tokenStart &&
           (tokenEnd[-1] == ' ' || tokenEnd[-1] == '\t')) --tokenEnd;
    bool gzip = static_cast<size_t>(tokenEnd - tokenStart) == 4U;
    for (size_t index = 0; gzip && index < 4U; ++index) {
      gzip = tolower(static_cast<unsigned char>(tokenStart[index])) ==
             "gzip"[index];
    }
    bool rejected = false;
    while (*value != '\0' && *value != ',') {
      if (*value++ != ';') continue;
      while (*value == ' ' || *value == '\t') ++value;
      if ((value[0] == 'q' || value[0] == 'Q') && value[1] == '=') {
        value += 2;
        while (*value == ' ' || *value == '\t') ++value;
        bool valid = false;
        bool nonzero = false;
        for (const char *quality = value;
             *quality != '\0' && *quality != ',' && *quality != ';' &&
             *quality != ' ' && *quality != '\t';
             ++quality) {
          if (*quality == '.') continue;
          if (*quality < '0' || *quality > '9') {
            valid = false;
            nonzero = false;
            break;
          }
          valid = true;
          if (*quality != '0') nonzero = true;
        }
        rejected = !valid || !nonzero;
      }
    }
    if (gzip && !rejected) return true;
    if (*value == ',') ++value;
  }
  return false;
}
}  // namespace

void HttpTransportCore::setSecurityContext(const char *localHost,
                                           const char *csrfToken) {
  snprintf(localHost_, sizeof(localHost_), "%s", localHost == nullptr ? "" : localHost);
  snprintf(expectedOrigin_, sizeof(expectedOrigin_), "http://%s", localHost_);
  snprintf(csrfToken_, sizeof(csrfToken_), "%s", csrfToken == nullptr ? "" : csrfToken);
  snprintf(tokenBody_, sizeof(tokenBody_), "{\"schema\":1,\"csrf_token\":\"%s\"}\n", csrfToken_);
}

bool HttpTransportCore::beginConnection(uint32_t nowMillis) {
  if (active()) return false;
  resetParser(nowMillis);
  return true;
}

void HttpTransportCore::resetParser(uint32_t nowMillis) {
  state_ = HttpTransportState::REQUEST_LINE;
  connectedMillis_ = nowMillis;
  lastActivityMillis_ = nowMillis;
  responseStartedMillis_ = 0;
  responseLifetimeMillis_ = JSON_RESPONSE_LIFETIME_MS;
  requestLineLength_ = 0;
  headerBytes_ = 0;
  headerLineLength_ = 0;
  headerHasColon_ = false;
  sawCarriageReturn_ = false;
  contentLength_ = 0;
  requestBodyLength_ = 0;
  contentLengthSeen_ = contentTypeSeen_ = contentTypeValid_ = false;
  hostSeen_ = originSeen_ = csrfSeen_ = sessionSeen_ = false;
  hostValid_ = originValid_ = csrfValid_ = false;
  sessionHeader_[0] = '\0';
  transferEncodingSeen_ = false;
  gzipAccepted_ = false;
  responseHeaderLength_ = 0;
  responseHeaderOffset_ = 0;
  responseBody_ = nullptr;
  responseBodyLength_ = 0;
  responseBodyOffset_ = 0;
  statusCode_ = 0;
  responseBodyStorage_ = HttpBodyStorage::RAM;
  route_ = Route::NONE;
}

void HttpTransportCore::disconnect() {
  state_ = HttpTransportState::DISCONNECTED;
  responseBody_ = nullptr;
}

bool HttpTransportCore::consume(uint8_t value, uint32_t nowMillis) {
  if (state_ != HttpTransportState::REQUEST_LINE &&
      state_ != HttpTransportState::HEADERS &&
      state_ != HttpTransportState::BODY) {
    return false;
  }
  lastActivityMillis_ = nowMillis;

  if (state_ == HttpTransportState::BODY) {
    if (requestBodyLength_ >= contentLength_ ||
        requestBodyLength_ >= MAX_REQUEST_BODY_BYTES) return false;
    requestBody_[requestBodyLength_++] = static_cast<char>(value);
    if (requestBodyLength_ == contentLength_) finishBody(nowMillis);
    return true;
  }

  if (state_ == HttpTransportState::REQUEST_LINE) {
    if (value == '\r') {
      sawCarriageReturn_ = true;
      return true;
    }
    if (value == '\n') {
      if (!sawCarriageReturn_ || requestLineLength_ == 0) {
        prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                        BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
      } else {
        requestLine_[requestLineLength_] = '\0';
        finishRequestLine();
      }
      sawCarriageReturn_ = false;
      return true;
    }
    if (sawCarriageReturn_) {
      prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                      BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
      return true;
    }
    if (requestLineLength_ >= MAX_REQUEST_LINE_BYTES) {
      prepareResponse(414, "URI Too Long", "text/plain; charset=utf-8",
                      REQUEST_LINE_TOO_LONG_BODY,
                      sizeof(REQUEST_LINE_TOO_LONG_BODY) - 1U);
      return true;
    }
    requestLine_[requestLineLength_++] = static_cast<char>(value);
    return true;
  }

  ++headerBytes_;
  if (headerBytes_ > MAX_HEADER_BYTES) {
    prepareResponse(431, "Request Header Fields Too Large",
                    "text/plain; charset=utf-8", HEADERS_TOO_LARGE_BODY,
                    sizeof(HEADERS_TOO_LARGE_BODY) - 1U);
    return true;
  }
  if (value == '\r') {
    sawCarriageReturn_ = true;
    return true;
  }
  if (value == '\n') {
    if (!sawCarriageReturn_) {
      prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                      BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    } else {
      finishHeaderLine(nowMillis);
    }
    sawCarriageReturn_ = false;
    return true;
  }
  if (sawCarriageReturn_) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return true;
  }
  ++headerLineLength_;
  if (headerLineLength_ >= sizeof(headerLine_)) {
    prepareResponse(431, "Request Header Fields Too Large",
                    "text/plain; charset=utf-8", HEADERS_TOO_LARGE_BODY,
                    sizeof(HEADERS_TOO_LARGE_BODY) - 1U);
    return true;
  }
  headerLine_[headerLineLength_ - 1U] = static_cast<char>(value);
  if (value == ':') headerHasColon_ = true;
  return true;
}

void HttpTransportCore::finishRequestLine() {
  char *firstSpace = strchr(requestLine_, ' ');
  if (firstSpace == nullptr || firstSpace == requestLine_) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return;
  }
  *firstSpace = '\0';
  char *path = firstSpace + 1;
  char *secondSpace = strchr(path, ' ');
  if (secondSpace == nullptr || secondSpace == path) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return;
  }
  *secondSpace = '\0';
  const char *version = secondSpace + 1;
  if ((strcmp(version, "HTTP/1.1") != 0 &&
       strcmp(version, "HTTP/1.0") != 0) || strchr(version, ' ') != nullptr) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return;
  }
  const bool isGet = strcmp(requestLine_, "GET") == 0;
  const bool isPut = strcmp(requestLine_, "PUT") == 0;
  const bool isPost = strcmp(requestLine_, "POST") == 0;
  if (!isGet && !isPut && !isPost) {
    prepareResponse(405, "Method Not Allowed", "text/plain; charset=utf-8",
                    METHOD_NOT_ALLOWED_BODY,
                    sizeof(METHOD_NOT_ALLOWED_BODY) - 1U);
    return;
  }

  if (profile_ == Profile::PROVISIONING && isGet && strcmp(path, "/") == 0) {
    route_ = Route::PROVISIONING_PAGE;
    statusCode_ = 200;
  } else if (profile_ == Profile::PROVISIONING && isGet &&
             strcmp(path, "/api/provision/v1/token") == 0) {
    route_ = Route::PROVISIONING_TOKEN;
    statusCode_ = 200;
  } else if (profile_ == Profile::PROVISIONING && isPost &&
             strcmp(path, "/api/provision/v1/wifi") == 0) {
    route_ = Route::SAVE_WIFI;
  } else if (profile_ == Profile::PROVISIONING && isGet &&
             strcmp(path, "/api/provision/v1/scan") == 0) {
    route_ = Route::PROVISIONING_SCAN_GET;
    statusCode_ = 200;
  } else if (profile_ == Profile::PROVISIONING && isPost &&
             strcmp(path, "/api/provision/v1/scan") == 0) {
    route_ = Route::PROVISIONING_SCAN_POST;
  } else if (profile_ == Profile::PROVISIONING && isPost &&
             strcmp(path, "/api/provision/v1/operational") == 0) {
    route_ = Route::ENTER_AP_OPERATIONAL;
  } else if (profile_ == Profile::PROVISIONING) {
    route_ = Route::NOT_FOUND;
    responseBody_ = NOT_FOUND_BODY;
    responseBodyLength_ = sizeof(NOT_FOUND_BODY) - 1U;
    statusCode_ = 404;
  } else if (isGet && strcmp(path, "/") == 0) {
    route_ = Route::DASHBOARD;
    responseBody_ = dashboardPageData();
    responseBodyLength_ = dashboardPageSize();
    responseBodyStorage_ = HttpBodyStorage::PROGRAM_MEMORY;
  } else if (isGet && strcmp(path, "/config") == 0) {
    route_ = Route::CONFIGURATION_PAGE;
    responseBody_ = configurationPageData();
    responseBodyLength_ = configurationPageSize();
    responseBodyStorage_ = HttpBodyStorage::PROGRAM_MEMORY;
    statusCode_ = 200;
  } else if (isGet && strcmp(path, "/api/status") == 0) {
    route_ = Route::STATUS;
    responseBody_ = STATUS_BODY;
    responseBodyLength_ = sizeof(STATUS_BODY) - 1U;
    statusCode_ = 200;
  } else if (isGet && strcmp(path, "/api/config/v1") == 0) {
    route_ = Route::CONFIG;
    statusCode_ = 200;
  } else if (isGet && strcmp(path, "/api/config/v1/token") == 0) {
    route_ = Route::TOKEN;
    statusCode_ = 200;
  } else if (isGet && strcmp(path, "/api/config/v1/session") == 0) {
    route_ = Route::SESSION;
    statusCode_ = 200;
  } else if (isPost && strcmp(path, "/api/config/v1/wifi/setup") == 0) {
    route_ = Route::ENTER_AP_SETUP;
  } else if (isPut && strcmp(path, "/api/config/v1/candidate") == 0) {
    route_ = Route::CANDIDATE;
  } else if (isPost && strcmp(path, "/api/config/v1/discard") == 0) {
    route_ = Route::DISCARD;
  } else if (isPost && strcmp(path, "/api/config/v1/save") == 0) {
    route_ = Route::SAVE_WSPR;
  } else if (isPost && strcmp(path, "/api/config/v1/session/enter") == 0) {
    route_ = Route::SESSION_ENTER;
  } else if (isPost && strcmp(path, "/api/config/v1/session/heartbeat") == 0) {
    route_ = Route::SESSION_HEARTBEAT;
  } else if (isPost && strcmp(path, "/api/config/v1/session/exit") == 0) {
    route_ = Route::SESSION_EXIT;
  } else if (isPost && strcmp(path, "/api/tx/v1/on") == 0) {
    route_ = Route::MANUAL_TX_ON;
  } else if (isPost && strcmp(path, "/api/tx/v1/off") == 0) {
    route_ = Route::MANUAL_TX_OFF;
  } else if (isPost && strcmp(path, "/api/factory/v1/reset") == 0) {
    route_ = Route::FACTORY_RESET;
  } else if (isPut || isPost) {
    prepareResponse(405, "Method Not Allowed", "text/plain; charset=utf-8",
                    METHOD_NOT_ALLOWED_BODY, sizeof(METHOD_NOT_ALLOWED_BODY) - 1U);
    return;
  } else {
    route_ = Route::NOT_FOUND;
    responseBody_ = NOT_FOUND_BODY;
    responseBodyLength_ = sizeof(NOT_FOUND_BODY) - 1U;
    statusCode_ = 404;
  }
  state_ = HttpTransportState::HEADERS;
  headerLineLength_ = 0;
  headerHasColon_ = false;
}

void HttpTransportCore::finishHeaderLine(uint32_t nowMillis) {
  if (headerLineLength_ == 0) {
    const bool mutation = route_ == Route::CANDIDATE || route_ == Route::DISCARD ||
                          route_ == Route::SAVE_WSPR ||
                          route_ == Route::SAVE_WIFI ||
                          route_ == Route::PROVISIONING_SCAN_POST ||
                          route_ == Route::ENTER_AP_SETUP ||
                          route_ == Route::ENTER_AP_OPERATIONAL ||
                          route_ == Route::SESSION_ENTER ||
                          route_ == Route::SESSION_HEARTBEAT ||
                          route_ == Route::SESSION_EXIT ||
                          route_ == Route::MANUAL_TX_ON ||
                          route_ == Route::MANUAL_TX_OFF ||
                          route_ == Route::FACTORY_RESET;
    if ((profile_ == Profile::PROVISIONING ||
         profile_ == Profile::AP_OPERATIONAL || route_ == Route::TOKEN) &&
        (!hostSeen_ || !hostValid_)) {
      prepareResponse(403, "Forbidden", "application/json; charset=utf-8",
                      FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY) - 1U);
      return;
    }
    if (mutation) {
      if (!hostSeen_ || !hostValid_ || !originSeen_ || !originValid_ ||
          !csrfSeen_ || !csrfValid_) {
        prepareResponse(403, "Forbidden", "application/json; charset=utf-8",
                        FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY) - 1U);
        return;
      }
      if (transferEncodingSeen_ || !contentLengthSeen_) {
        prepareResponse(400, "Bad Request", "application/json; charset=utf-8",
                        LENGTH_REQUIRED_BODY, sizeof(LENGTH_REQUIRED_BODY) - 1U);
        return;
      }
      if (!contentTypeSeen_ || !contentTypeValid_) {
        prepareResponse(415, "Unsupported Media Type", "application/json; charset=utf-8",
                        UNSUPPORTED_MEDIA_BODY, sizeof(UNSUPPORTED_MEDIA_BODY) - 1U);
        return;
      }
      if (contentLength_ > MAX_REQUEST_BODY_BYTES) {
        prepareResponse(413, "Payload Too Large", "application/json; charset=utf-8",
                        BODY_TOO_LARGE, sizeof(BODY_TOO_LARGE) - 1U);
        return;
      }
      if (contentLength_ == 0) {
        prepareResponse(400, "Bad Request", "application/json; charset=utf-8",
                        BAD_REQUEST_JSON, sizeof(BAD_REQUEST_JSON) - 1U);
        return;
      }
      state_ = HttpTransportState::BODY;
      return;
    }
    if (route_ == Route::PROVISIONING_PAGE) {
      const bool gzip = gzipAccepted_;
      prepareResponse(200, "OK", "text/html; charset=utf-8",
                      gzip ? provisioningPageGzipData()
                           : provisioningPageData(),
                      gzip ? provisioningPageGzipSize()
                           : provisioningPageSize(),
                      HttpBodyStorage::PROGRAM_MEMORY, gzip, true);
    } else if (route_ == Route::DASHBOARD || route_ == Route::CONFIGURATION_PAGE) {
      const bool configurationPage = route_ == Route::CONFIGURATION_PAGE;
      const bool gzip = gzipAccepted_;
      prepareResponse(200, "OK", "text/html; charset=utf-8",
                      configurationPage
                          ? (gzip ? configurationPageGzipData()
                                  : configurationPageData())
                          : (gzip ? dashboardPageGzipData()
                                  : dashboardPageData()),
                      configurationPage
                          ? (gzip ? configurationPageGzipSize()
                                  : configurationPageSize())
                          : (gzip ? dashboardPageGzipSize()
                                  : dashboardPageSize()),
                      HttpBodyStorage::PROGRAM_MEMORY, gzip, true);
    } else if (route_ == Route::STATUS) {
      const char *body = STATUS_BODY;
      size_t bodyLength = sizeof(STATUS_BODY) - 1U;
      if (statusBodyProvider_ != nullptr &&
          (!statusBodyProvider_(statusBodyContext_, nowMillis, body,
                                bodyLength) ||
           body == nullptr || bodyLength == 0)) {
        prepareResponse(503, "Service Unavailable",
                        "application/json; charset=utf-8",
                        STATUS_UNAVAILABLE_BODY,
                        sizeof(STATUS_UNAVAILABLE_BODY) - 1U);
      } else {
        prepareResponse(200, "OK", "application/json; charset=utf-8",
                        body, bodyLength);
      }
    } else if (route_ == Route::CONFIG) {
      const char *body = nullptr;
      size_t bodyLength = 0;
      if (configBodyProvider_ == nullptr ||
          !configBodyProvider_(configBodyContext_, nowMillis, body,
                               bodyLength) ||
          body == nullptr || bodyLength == 0) {
        prepareResponse(503, "Service Unavailable",
                        "application/json; charset=utf-8",
                        CONFIG_UNAVAILABLE_BODY,
                        sizeof(CONFIG_UNAVAILABLE_BODY) - 1U);
      } else {
        prepareResponse(200, "OK", "application/json; charset=utf-8",
                        body, bodyLength);
      }
    } else if (route_ == Route::SESSION) {
      const char *body = nullptr;
      size_t bodyLength = 0;
      if (sessionBodyProvider_ == nullptr ||
          !sessionBodyProvider_(sessionBodyContext_, nowMillis, body,
                                bodyLength) ||
          body == nullptr || bodyLength == 0) {
        prepareResponse(503, "Service Unavailable",
                        "application/json; charset=utf-8",
                        SESSION_UNAVAILABLE_BODY,
                        sizeof(SESSION_UNAVAILABLE_BODY) - 1U);
      } else {
        prepareResponse(200, "OK", "application/json; charset=utf-8",
                        body, bodyLength);
      }
    } else if (route_ == Route::PROVISIONING_SCAN_GET) {
      const char *body = nullptr;
      size_t bodyLength = 0;
      if (scanBodyProvider_ == nullptr ||
          !scanBodyProvider_(scanBodyContext_, nowMillis, body, bodyLength) ||
          body == nullptr || bodyLength == 0) {
        prepareResponse(503, "Service Unavailable",
                        "application/json; charset=utf-8",
                        STATUS_UNAVAILABLE_BODY,
                        sizeof(STATUS_UNAVAILABLE_BODY) - 1U);
      } else {
        prepareResponse(200, "OK", "application/json; charset=utf-8",
                        body, bodyLength);
      }
    } else if (route_ == Route::TOKEN ||
               route_ == Route::PROVISIONING_TOKEN) {
      prepareResponse(200, "OK", "application/json; charset=utf-8",
                      tokenBody_, strlen(tokenBody_));
    } else {
      prepareResponse(404, "Not Found", "text/plain; charset=utf-8",
                      NOT_FOUND_BODY, sizeof(NOT_FOUND_BODY) - 1U);
    }
    return;
  }
  if (!headerHasColon_) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return;
  }
  headerLine_[headerLineLength_] = '\0';
  char *colon = strchr(headerLine_, ':');
  if (colon == nullptr) {
    prepareResponse(400, "Bad Request", "text/plain; charset=utf-8",
                    BAD_REQUEST_BODY, sizeof(BAD_REQUEST_BODY) - 1U);
    return;
  }
  *colon = '\0';
  char *value = colon + 1;
  while (*value == ' ' || *value == '\t') ++value;
  char *end = value + strlen(value);
  while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
  if (equalIgnoreCase(headerLine_, "Host")) {
    if (hostSeen_) { prepareResponse(403, "Forbidden", "application/json; charset=utf-8", FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY)-1U); return; }
    hostSeen_ = true; hostValid_ = strcmp(value, localHost_) == 0;
  } else if (equalIgnoreCase(headerLine_, "Origin")) {
    if (originSeen_) { prepareResponse(403, "Forbidden", "application/json; charset=utf-8", FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY)-1U); return; }
    originSeen_ = true; originValid_ = strcmp(value, expectedOrigin_) == 0;
  } else if (equalIgnoreCase(headerLine_, "X-WSPR-CSRF-Token")) {
    if (csrfSeen_) { prepareResponse(403, "Forbidden", "application/json; charset=utf-8", FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY)-1U); return; }
    csrfSeen_ = true; csrfValid_ = constantTimeEqual(value, csrfToken_);
  } else if (equalIgnoreCase(headerLine_, "X-WSPR-Session")) {
    if (sessionSeen_ || strlen(value) > 32U) { prepareResponse(403, "Forbidden", "application/json; charset=utf-8", FORBIDDEN_BODY, sizeof(FORBIDDEN_BODY)-1U); return; }
    sessionSeen_ = true;
    memcpy(sessionHeader_, value, strlen(value) + 1U);
  } else if (equalIgnoreCase(headerLine_, "Content-Type")) {
    if (contentTypeSeen_) { prepareResponse(400, "Bad Request", "application/json; charset=utf-8", BAD_REQUEST_JSON, sizeof(BAD_REQUEST_JSON)-1U); return; }
    contentTypeSeen_ = true;
    contentTypeValid_ = strcmp(value, "application/json") == 0;
  } else if (equalIgnoreCase(headerLine_, "Content-Length")) {
    if (contentLengthSeen_ || *value == '\0') { prepareResponse(400, "Bad Request", "application/json; charset=utf-8", BAD_REQUEST_JSON, sizeof(BAD_REQUEST_JSON)-1U); return; }
    size_t parsed = 0;
    for (const char *p = value; *p; ++p) {
      if (*p < '0' || *p > '9' || parsed > (SIZE_MAX - static_cast<size_t>(*p - '0')) / 10U) {
        prepareResponse(400, "Bad Request", "application/json; charset=utf-8", BAD_REQUEST_JSON, sizeof(BAD_REQUEST_JSON)-1U); return;
      }
      parsed = parsed * 10U + static_cast<size_t>(*p - '0');
    }
    contentLengthSeen_ = true; contentLength_ = parsed;
  } else if (equalIgnoreCase(headerLine_, "Transfer-Encoding")) {
    transferEncodingSeen_ = true;
  } else if (equalIgnoreCase(headerLine_, "Accept-Encoding")) {
    gzipAccepted_ = gzipAccepted_ || acceptsGzip(value);
  }
  headerLineLength_ = 0;
  headerHasColon_ = false;
}

void HttpTransportCore::finishBody(uint32_t nowMillis) {
  requestBody_[requestBodyLength_] = '\0';
  if (mutationProvider_ == nullptr) {
    prepareResponse(503, "Service Unavailable", "application/json; charset=utf-8",
                    MUTATION_UNAVAILABLE_BODY, sizeof(MUTATION_UNAVAILABLE_BODY) - 1U);
    return;
  }
  uint16_t code = 503;
  const char *body = MUTATION_UNAVAILABLE_BODY;
  size_t length = sizeof(MUTATION_UNAVAILABLE_BODY) - 1U;
  Mutation mutation = Mutation::SESSION_EXIT;
  if (route_ == Route::CANDIDATE) mutation = Mutation::STAGE_CANDIDATE;
  else if (route_ == Route::DISCARD) mutation = Mutation::DISCARD;
  else if (route_ == Route::SAVE_WSPR) mutation = Mutation::SAVE_WSPR;
  else if (route_ == Route::SAVE_WIFI) mutation = Mutation::SAVE_WIFI;
  else if (route_ == Route::PROVISIONING_SCAN_POST) mutation = Mutation::START_SCAN;
  else if (route_ == Route::ENTER_AP_SETUP) mutation = Mutation::ENTER_AP_SETUP;
  else if (route_ == Route::ENTER_AP_OPERATIONAL) mutation = Mutation::ENTER_AP_OPERATIONAL;
  else if (route_ == Route::MANUAL_TX_ON) mutation = Mutation::MANUAL_TX_ON;
  else if (route_ == Route::MANUAL_TX_OFF) mutation = Mutation::MANUAL_TX_OFF;
  else if (route_ == Route::FACTORY_RESET) mutation = Mutation::FACTORY_RESET;
  else if (route_ == Route::SESSION_ENTER) mutation = Mutation::SESSION_ENTER;
  else if (route_ == Route::SESSION_HEARTBEAT) mutation = Mutation::SESSION_HEARTBEAT;
  mutationProvider_(mutationContext_, mutation, requestBody_, requestBodyLength_,
                    sessionSeen_ ? sessionHeader_ : nullptr, nowMillis, code,
                    body, length);
  volatile char *sensitiveBody = requestBody_;
  for (size_t index = 0; index < requestBodyLength_; ++index) {
    sensitiveBody[index] = '\0';
  }
  requestBodyLength_ = 0;
  const char *reason = code == 200 ? "OK" : code == 202 ? "Accepted" : code == 400 ? "Bad Request" :
      code == 403 ? "Forbidden" : code == 409 ? "Conflict" :
      code == 413 ? "Payload Too Large" : code == 415 ? "Unsupported Media Type" :
      code == 422 ? "Unprocessable Entity" :
      code == 500 ? "Internal Server Error" : "Service Unavailable";
  prepareResponse(code, reason, "application/json; charset=utf-8", body, length);
}

void HttpTransportCore::prepareResponse(uint16_t statusCode,
                                        const char *reason,
                                        const char *contentType,
                                        const char *body,
                                        size_t bodyLength,
                                        HttpBodyStorage storage,
                                        bool gzipEncoded,
                                        bool variesByAcceptEncoding) {
  statusCode_ = statusCode;
  responseBody_ = body;
  responseBodyLength_ = bodyLength;
  responseBodyStorage_ = storage;
  responseStartedMillis_ = lastActivityMillis_;
  responseLifetimeMillis_ =
      storage == HttpBodyStorage::PROGRAM_MEMORY
          ? HTML_RESPONSE_LIFETIME_MS
          : JSON_RESPONSE_LIFETIME_MS;
  responseBodyOffset_ = 0;
  responseHeaderOffset_ = 0;
  const int written = snprintf(
      responseHeader_, sizeof(responseHeader_),
      "HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
      "Connection: close\r\nCache-Control: no-store\r\n%s%s"
      "X-Content-Type-Options: nosniff\r\n\r\n",
      static_cast<unsigned>(statusCode), reason, contentType,
      static_cast<unsigned>(bodyLength),
      gzipEncoded ? "Content-Encoding: gzip\r\n" : "",
      variesByAcceptEncoding ? "Vary: Accept-Encoding\r\n" : "");
  responseHeaderLength_ = written > 0 &&
                                  static_cast<size_t>(written) <
                                      sizeof(responseHeader_)
                              ? static_cast<size_t>(written)
                              : 0;
  state_ = responseHeaderLength_ == 0 ? HttpTransportState::COMPLETE
                                     : HttpTransportState::RESPONDING;
}

HttpBodyStorage HttpTransportCore::outputStorage() const {
  if (responseHeaderOffset_ < responseHeaderLength_) {
    return HttpBodyStorage::RAM;
  }
  return responseBodyStorage_;
}

bool HttpTransportCore::timedOut(uint32_t nowMillis) const {
  return timeoutKind(nowMillis) != HttpTimeoutKind::NONE;
}

HttpTimeoutKind HttpTransportCore::timeoutKind(uint32_t nowMillis) const {
  if (!active() || complete()) return HttpTimeoutKind::NONE;
  if (responseReady()) {
    if (static_cast<uint32_t>(nowMillis - responseStartedMillis_) >=
        responseLifetimeMillis_) return HttpTimeoutKind::RESPONSE_TOTAL;
    return HttpTimeoutKind::NONE;
  }
  if (static_cast<uint32_t>(nowMillis - connectedMillis_) >=
      CLIENT_LIFETIME_MS) return HttpTimeoutKind::REQUEST_TOTAL;
  if (static_cast<uint32_t>(nowMillis - lastActivityMillis_) >=
      CLIENT_INACTIVITY_MS) return HttpTimeoutKind::REQUEST_INACTIVITY;
  return HttpTimeoutKind::NONE;
}

const uint8_t *HttpTransportCore::outputData(size_t &available) const {
  available = 0;
  if (!responseReady()) return nullptr;
  if (responseHeaderOffset_ < responseHeaderLength_) {
    available = responseHeaderLength_ - responseHeaderOffset_;
    return reinterpret_cast<const uint8_t *>(responseHeader_ +
                                             responseHeaderOffset_);
  }
  if (responseBodyOffset_ < responseBodyLength_) {
    available = responseBodyLength_ - responseBodyOffset_;
    return reinterpret_cast<const uint8_t *>(responseBody_ +
                                             responseBodyOffset_);
  }
  return nullptr;
}

void HttpTransportCore::consumeOutput(size_t count, uint32_t nowMillis) {
  if (!responseReady() || count == 0) return;
  (void)nowMillis;
  if (responseHeaderOffset_ < responseHeaderLength_) {
    const size_t remaining = responseHeaderLength_ - responseHeaderOffset_;
    responseHeaderOffset_ += count < remaining ? count : remaining;
  } else {
    const size_t remaining = responseBodyLength_ - responseBodyOffset_;
    responseBodyOffset_ += count < remaining ? count : remaining;
  }
  if (responseHeaderOffset_ >= responseHeaderLength_ &&
      responseBodyOffset_ >= responseBodyLength_) {
    state_ = HttpTransportState::COMPLETE;
  }
}
