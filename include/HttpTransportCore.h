#pragma once

#include <stddef.h>
#include <stdint.h>

enum class HttpTransportState : uint8_t {
  DISCONNECTED,
  REQUEST_LINE,
  HEADERS,
  BODY,
  RESPONDING,
  COMPLETE
};
enum class HttpBodyStorage : uint8_t { RAM, PROGRAM_MEMORY };
enum class HttpTimeoutKind : uint8_t {
  NONE,
  REQUEST_TOTAL,
  REQUEST_INACTIVITY,
  RESPONSE_TOTAL
};
class HttpTransportCore {
 public:
  enum class Profile : uint8_t { NORMAL, PROVISIONING, AP_OPERATIONAL };
  using JsonBodyProvider = bool (*)(void *context, uint32_t nowMillis,
                                    const char *&body, size_t &length);
  enum class Mutation : uint8_t {
    STAGE_CANDIDATE,
    DISCARD,
    SESSION_ENTER,
    SESSION_HEARTBEAT,
    SESSION_EXIT,
    SAVE_WSPR,
    SAVE_WIFI,
    START_SCAN,
    ENTER_AP_SETUP,
    ENTER_AP_OPERATIONAL,
    MANUAL_TX_ON,
    MANUAL_TX_OFF
    ,FACTORY_RESET
  };
  using MutationProvider = void (*)(void *context, Mutation mutation,
                                    const char *requestBody, size_t requestLength,
                                    const char *sessionHeader,
                                    uint32_t nowMillis, uint16_t &statusCode,
                                    const char *&responseBody,
                                    size_t &responseLength);
  static constexpr size_t MAX_REQUEST_LINE_BYTES = 128;
  static constexpr size_t MAX_HEADER_BYTES = 512;
  static constexpr uint32_t CLIENT_LIFETIME_MS = 600UL;
  static constexpr uint32_t CLIENT_INACTIVITY_MS = 200UL;
  static constexpr uint32_t JSON_RESPONSE_LIFETIME_MS = 5000UL;
  static constexpr uint32_t HTML_RESPONSE_LIFETIME_MS = 15000UL;
  static constexpr uint8_t LISTENER_BACKLOG = 1;
  static constexpr size_t MAX_REQUEST_BODY_BYTES = 512;

  bool beginConnection(uint32_t nowMillis);
  void setStatusBodyProvider(JsonBodyProvider provider, void *context) {
    statusBodyProvider_ = provider;
    statusBodyContext_ = context;
  }
  void setConfigBodyProvider(JsonBodyProvider provider, void *context) {
    configBodyProvider_ = provider;
    configBodyContext_ = context;
  }
  void setSessionBodyProvider(JsonBodyProvider provider, void *context) {
    sessionBodyProvider_ = provider;
    sessionBodyContext_ = context;
  }
  void setScanBodyProvider(JsonBodyProvider provider, void *context) {
    scanBodyProvider_ = provider;
    scanBodyContext_ = context;
  }
  void setMutationProvider(MutationProvider provider, void *context) {
    mutationProvider_ = provider; mutationContext_ = context;
  }
  void setSecurityContext(const char *localHost, const char *csrfToken);
  void setProfile(Profile profile) { profile_ = profile; }
  void disconnect();
  bool consume(uint8_t value, uint32_t nowMillis);
  bool timedOut(uint32_t nowMillis) const;
  HttpTimeoutKind timeoutKind(uint32_t nowMillis) const;

  const uint8_t *outputData(size_t &available) const;
  HttpBodyStorage outputStorage() const;
  void consumeOutput(size_t count, uint32_t nowMillis);

  bool active() const {
    return state_ != HttpTransportState::DISCONNECTED;
  }
  bool responseReady() const {
    return state_ == HttpTransportState::RESPONDING;
  }
  bool complete() const { return state_ == HttpTransportState::COMPLETE; }
  uint32_t responseStartedMillis() const { return responseStartedMillis_; }
  size_t responseBytesSent() const {
    return responseHeaderOffset_ + responseBodyOffset_;
  }
  HttpTransportState state() const { return state_; }
  uint16_t statusCode() const { return statusCode_; }
  bool apOperationalTransitionResponse() const {
    return route_ == Route::ENTER_AP_OPERATIONAL && statusCode_ == 202;
  }
  bool wifiRestartResponse() const {
    return route_ == Route::ENTER_AP_SETUP && statusCode_ == 202;
  }

 private:
  enum class Route : uint8_t { NONE, DASHBOARD, CONFIGURATION_PAGE, STATUS, CONFIG, TOKEN,
                               SESSION, CANDIDATE, DISCARD, SESSION_ENTER,
                               SESSION_HEARTBEAT, SESSION_EXIT, SAVE_WSPR,
                               PROVISIONING_PAGE, PROVISIONING_TOKEN, SAVE_WIFI,
                               PROVISIONING_SCAN_GET, PROVISIONING_SCAN_POST,
                               ENTER_AP_SETUP,
                               ENTER_AP_OPERATIONAL,
                               MANUAL_TX_ON, MANUAL_TX_OFF,
                               FACTORY_RESET,
                               NOT_FOUND };
  static constexpr size_t RESPONSE_HEADER_BYTES = 224;

  HttpTransportState state_ = HttpTransportState::DISCONNECTED;
  uint32_t connectedMillis_ = 0;
  uint32_t lastActivityMillis_ = 0;
  uint32_t responseStartedMillis_ = 0;
  uint32_t responseLifetimeMillis_ = JSON_RESPONSE_LIFETIME_MS;
  char requestLine_[MAX_REQUEST_LINE_BYTES + 1] = {};
  char headerLine_[193] = {};
  char requestBody_[MAX_REQUEST_BODY_BYTES + 1U] = {};
  char localHost_[16] = {};
  char expectedOrigin_[24] = {};
  char csrfToken_[33] = {};
  char sessionHeader_[33] = {};
  char tokenBody_[64] = {};
  size_t requestLineLength_ = 0;
  size_t headerBytes_ = 0;
  size_t headerLineLength_ = 0;
  bool headerHasColon_ = false;
  bool sawCarriageReturn_ = false;
  size_t contentLength_ = 0;
  size_t requestBodyLength_ = 0;
  bool contentLengthSeen_ = false;
  bool contentTypeSeen_ = false;
  bool contentTypeValid_ = false;
  bool hostSeen_ = false;
  bool originSeen_ = false;
  bool csrfSeen_ = false;
  bool sessionSeen_ = false;
  bool hostValid_ = false;
  bool originValid_ = false;
  bool csrfValid_ = false;
  bool transferEncodingSeen_ = false;
  bool gzipAccepted_ = false;
  char responseHeader_[RESPONSE_HEADER_BYTES] = {};
  size_t responseHeaderLength_ = 0;
  size_t responseHeaderOffset_ = 0;
  const char *responseBody_ = nullptr;
  size_t responseBodyLength_ = 0;
  size_t responseBodyOffset_ = 0;
  uint16_t statusCode_ = 0;
  HttpBodyStorage responseBodyStorage_ = HttpBodyStorage::RAM;
  Route route_ = Route::NONE;
  Profile profile_ = Profile::NORMAL;
  JsonBodyProvider statusBodyProvider_ = nullptr;
  void *statusBodyContext_ = nullptr;
  JsonBodyProvider configBodyProvider_ = nullptr;
  void *configBodyContext_ = nullptr;
  JsonBodyProvider sessionBodyProvider_ = nullptr;
  void *sessionBodyContext_ = nullptr;
  JsonBodyProvider scanBodyProvider_ = nullptr;
  void *scanBodyContext_ = nullptr;
  MutationProvider mutationProvider_ = nullptr;
  void *mutationContext_ = nullptr;

  void resetParser(uint32_t nowMillis);
  void finishRequestLine();
  void finishHeaderLine(uint32_t nowMillis);
  void finishBody(uint32_t nowMillis);
  void prepareResponse(uint16_t statusCode, const char *reason,
                       const char *contentType, const char *body,
                       size_t bodyLength,
                       HttpBodyStorage storage = HttpBodyStorage::RAM,
                       bool gzipEncoded = false,
                       bool variesByAcceptEncoding = false);
};
