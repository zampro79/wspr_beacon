#pragma once

#include <stdint.h>

#include "BoardConfig.h"
#include "ApplicationStatusJson.h"
#include "ConfigurationJson.h"
#include "HttpServiceGate.h"
#include "HttpTransportCore.h"
#include "HttpWritePlanner.h"

#if WSPR_HAS_WIFI
  #if defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266WiFi.h>
  #else
    #include <WiFi.h>
  #endif
#endif

class BoundedHttpServer {
 public:
  using StatusSnapshotProvider = bool (*)(void *context, uint32_t nowMillis,
                                          char *destination, size_t capacity,
                                          size_t &length);
  using ConfigSnapshotProvider = StatusSnapshotProvider;
  using MutationProvider = void (*)(void *context,
      HttpTransportCore::Mutation mutation, const char *requestBody,
      size_t requestLength, const char *sessionHeader, uint32_t nowMillis,
      uint16_t &statusCode,
      char *destination, size_t capacity, size_t &length);
  static constexpr uint16_t PORT = 80;
  static constexpr size_t MAX_INPUT_BYTES_PER_UPDATE = 64;
  static constexpr size_t MAX_OUTPUT_BYTES_PER_UPDATE =
      HttpWritePlanner::MAX_CHUNK_BYTES;
  static constexpr uint32_t ESP8266_WRITE_TIMEOUT_MS = 50UL;

  BoundedHttpServer();
  void beginSecurity();
  void setProfile(HttpTransportCore::Profile profile) {
    profile_ = profile;
    core_.setProfile(profile);
  }
  bool transitionProfile(HttpTransportCore::Profile profile);
  bool takeApOperationalTransitionDelivered() {
    const bool delivered = apOperationalTransitionDelivered_;
    apOperationalTransitionDelivered_ = false;
    return delivered;
  }
  bool takeWifiRestartResponseDelivered() {
    const bool delivered = wifiRestartResponseDelivered_;
    wifiRestartResponseDelivered_ = false;
    return delivered;
  }
  void update(const HttpServiceGate::Decision &decision, uint32_t nowMillis);
  void setStatusSnapshotProvider(StatusSnapshotProvider provider,
                                 void *context) {
    statusSnapshotProvider_ = provider;
    statusSnapshotContext_ = context;
  }
  void setConfigSnapshotProvider(ConfigSnapshotProvider provider,
                                 void *context) {
    configSnapshotProvider_ = provider;
    configSnapshotContext_ = context;
  }
  void setSessionSnapshotProvider(ConfigSnapshotProvider provider,
                                  void *context) {
    sessionSnapshotProvider_ = provider;
    sessionSnapshotContext_ = context;
  }
  void setScanSnapshotProvider(ConfigSnapshotProvider provider,
                               void *context) {
    scanSnapshotProvider_ = provider;
    scanSnapshotContext_ = context;
  }
  void setMutationProvider(MutationProvider provider, void *context) {
    mutationProvider_ = provider; mutationContext_ = context;
  }
  bool listenerRunning() const { return lifecycle_.listenerRunning(); }
  bool clientActive() const { return core_.active(); }
  const HttpDiagnosticsSnapshot &diagnostics() const {
    return diagnostics_.snapshot();
  }

 private:
  HttpTransportCore core_;
  HttpServiceLifecycle lifecycle_;
  HttpDiagnostics diagnostics_;
  char statusBody_[ApplicationStatusJson::MAX_RESPONSE_BYTES + 1U] = {};
  StatusSnapshotProvider statusSnapshotProvider_ = nullptr;
  void *statusSnapshotContext_ = nullptr;
  ConfigSnapshotProvider configSnapshotProvider_ = nullptr;
  void *configSnapshotContext_ = nullptr;
  ConfigSnapshotProvider sessionSnapshotProvider_ = nullptr;
  void *sessionSnapshotContext_ = nullptr;
  ConfigSnapshotProvider scanSnapshotProvider_ = nullptr;
  void *scanSnapshotContext_ = nullptr;
  MutationProvider mutationProvider_ = nullptr;
  void *mutationContext_ = nullptr;
  char csrfToken_[33] = {};
  char localHost_[16] = {};
  HttpTransportCore::Profile profile_ = HttpTransportCore::Profile::NORMAL;
  bool apOperationalTransitionDelivered_ = false;
  bool wifiRestartResponseDelivered_ = false;
#if WSPR_HAS_WIFI
  #if defined(ARDUINO_ARCH_ESP32)
  WiFiServer server_{PORT, HttpTransportCore::LISTENER_BACKLOG};
  #else
  WiFiServer server_{PORT};
  #endif
  WiFiClient client_;
  #if defined(ARDUINO_ARCH_ESP8266)
  uint8_t flashStaging_[HttpWritePlanner::MAX_CHUNK_BYTES] = {};
  #endif

  void startListener();
  void stopListener();
  void discardPendingClient();
  void acceptClient(uint32_t nowMillis);
  void serviceClient(uint32_t nowMillis);
  void closeClient(HttpClientCloseReason reason, uint32_t nowMillis);
#if defined(ARDUINO_ARCH_ESP32)
  size_t writeAvailable(const uint8_t *data, size_t length,
                        HttpBodyStorage storage, bool &disconnected);
  static int sendNonBlocking(void *context, int socketDescriptor,
                             const uint8_t *data, size_t length,
                             int &errorCode);
#else
  size_t writeAvailable(const uint8_t *data, size_t length,
                        HttpBodyStorage storage);
#endif
  static bool provideStatusBody(void *context, uint32_t nowMillis,
                                const char *&body, size_t &length);
  static bool provideConfigBody(void *context, uint32_t nowMillis,
                                const char *&body, size_t &length);
  static bool provideSessionBody(void *context, uint32_t nowMillis,
                                 const char *&body, size_t &length);
  static bool provideScanBody(void *context, uint32_t nowMillis,
                              const char *&body, size_t &length);
  static void performMutation(void *context, HttpTransportCore::Mutation mutation,
                              const char *requestBody, size_t requestLength,
                              const char *sessionHeader, uint32_t nowMillis,
                              uint16_t &statusCode,
                              const char *&body, size_t &length);
#endif
};
