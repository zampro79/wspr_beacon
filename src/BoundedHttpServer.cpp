#include "BoundedHttpServer.h"
#include <stdio.h>

#if WSPR_HAS_WIFI
  #if defined(ARDUINO_ARCH_ESP8266)
    #include <pgmspace.h>
    extern "C" {
      #include <user_interface.h>
    }
  #else
    #include <errno.h>
    #include <esp_system.h>
    #include <lwip/sockets.h>
    #include "HttpNonBlockingWriter.h"
  #endif

BoundedHttpServer::BoundedHttpServer() {
  static_assert(ConfigurationJson::MAX_RESPONSE_BYTES <=
                    ApplicationStatusJson::MAX_RESPONSE_BYTES,
                "Configuration JSON must reuse the existing response buffer");
  core_.setStatusBodyProvider(provideStatusBody, this);
  core_.setConfigBodyProvider(provideConfigBody, this);
  core_.setSessionBodyProvider(provideSessionBody, this);
  core_.setScanBodyProvider(provideScanBody, this);
  core_.setMutationProvider(performMutation, this);
}

void BoundedHttpServer::beginSecurity() {
  uint8_t randomBytes[16];
#if defined(ARDUINO_ARCH_ESP8266)
  os_get_random(randomBytes, sizeof(randomBytes));
#else
  esp_fill_random(randomBytes, sizeof(randomBytes));
#endif
  static const char HEX_DIGITS[] = "0123456789abcdef";
  for (size_t index = 0; index < sizeof(randomBytes); ++index) {
    csrfToken_[index * 2U] = HEX_DIGITS[randomBytes[index] >> 4U];
    csrfToken_[index * 2U + 1U] = HEX_DIGITS[randomBytes[index] & 0x0fU];
  }
  csrfToken_[32] = '\0';
}

bool BoundedHttpServer::provideStatusBody(void *context, uint32_t nowMillis,
                                          const char *&body, size_t &length) {
  BoundedHttpServer *server = static_cast<BoundedHttpServer *>(context);
  if (server == nullptr || server->statusSnapshotProvider_ == nullptr) {
    return false;
  }
  size_t generatedLength = 0;
  if (!server->statusSnapshotProvider_(
          server->statusSnapshotContext_, nowMillis, server->statusBody_,
          sizeof(server->statusBody_), generatedLength) ||
      generatedLength == 0 ||
      generatedLength > ApplicationStatusJson::MAX_RESPONSE_BYTES) {
    return false;
  }
  body = server->statusBody_;
  length = generatedLength;
  return true;
}

bool BoundedHttpServer::provideConfigBody(void *context, uint32_t nowMillis,
                                          const char *&body, size_t &length) {
  BoundedHttpServer *server = static_cast<BoundedHttpServer *>(context);
  if (server == nullptr || server->configSnapshotProvider_ == nullptr) {
    return false;
  }
  size_t generatedLength = 0;
  if (!server->configSnapshotProvider_(
          server->configSnapshotContext_, nowMillis, server->statusBody_,
          sizeof(server->statusBody_), generatedLength) ||
      generatedLength == 0 ||
      generatedLength > ConfigurationJson::MAX_RESPONSE_BYTES) {
    return false;
  }
  body = server->statusBody_;
  length = generatedLength;
  return true;
}

bool BoundedHttpServer::provideSessionBody(void *context, uint32_t nowMillis,
                                           const char *&body, size_t &length) {
  BoundedHttpServer *server = static_cast<BoundedHttpServer *>(context);
  if (server == nullptr || server->sessionSnapshotProvider_ == nullptr) return false;
  size_t generatedLength = 0;
  if (!server->sessionSnapshotProvider_(server->sessionSnapshotContext_,
          nowMillis, server->statusBody_, sizeof(server->statusBody_),
          generatedLength) || generatedLength == 0) return false;
  body = server->statusBody_;
  length = generatedLength;
  return true;
}

bool BoundedHttpServer::provideScanBody(void *context, uint32_t nowMillis,
                                        const char *&body, size_t &length) {
  BoundedHttpServer *server = static_cast<BoundedHttpServer *>(context);
  if (server == nullptr || server->scanSnapshotProvider_ == nullptr) return false;
  size_t generatedLength = 0;
  if (!server->scanSnapshotProvider_(server->scanSnapshotContext_, nowMillis,
          server->statusBody_, sizeof(server->statusBody_), generatedLength) ||
      generatedLength == 0 ||
      generatedLength > ApplicationStatusJson::MAX_RESPONSE_BYTES) return false;
  body = server->statusBody_;
  length = generatedLength;
  return true;
}

void BoundedHttpServer::performMutation(
    void *context, HttpTransportCore::Mutation mutation,
    const char *requestBody, size_t requestLength, const char *sessionHeader,
    uint32_t nowMillis,
    uint16_t &statusCode, const char *&body, size_t &length) {
  BoundedHttpServer *server = static_cast<BoundedHttpServer *>(context);
  statusCode = 503;
  length = 0;
  body = nullptr;
  if (server == nullptr || server->mutationProvider_ == nullptr) return;
  server->mutationProvider_(server->mutationContext_, mutation, requestBody,
                            requestLength, sessionHeader, nowMillis, statusCode,
                            server->statusBody_, sizeof(server->statusBody_),
                            length);
  if (length != 0 && length <= ApplicationStatusJson::MAX_RESPONSE_BYTES) {
    body = server->statusBody_;
  } else {
    length = 0;
  }
}

void BoundedHttpServer::startListener() {
#if defined(ARDUINO_ARCH_ESP8266)
  server_.begin(PORT, HttpTransportCore::LISTENER_BACKLOG);
#else
  server_.begin();
#endif
  // Keep normal TCP coalescing: the staged chunks are intentionally larger
  // than the previous small writes and do not benefit from forced no-delay.
  server_.setNoDelay(false);
  lifecycle_.listenerStarted();
  diagnostics_.listenerStarted();
}

void BoundedHttpServer::stopListener() {
  discardPendingClient();
  server_.stop();
  lifecycle_.listenerStopped();
  diagnostics_.listenerStopped();
}

bool BoundedHttpServer::transitionProfile(HttpTransportCore::Profile profile) {
  if (core_.active()) return false;
  stopListener();
  beginSecurity();
  setProfile(profile);
  return true;
}

void BoundedHttpServer::discardPendingClient() {
  if (!server_.hasClient()) return;
  WiFiClient pending = server_.accept();
  if (pending) pending.stop();
  diagnostics_.requestFailed();
  diagnostics_.forcedClosure();
}

void BoundedHttpServer::closeClient(HttpClientCloseReason reason,
                                    uint32_t nowMillis) {
  if (core_.responseReady()) {
    diagnostics_.responseInterrupted(nowMillis - core_.responseStartedMillis(),
                                      core_.responseBytesSent());
  }
  diagnostics_.clientClosed(reason);
  if (client_) client_.stop();
  client_ = WiFiClient();
  core_.disconnect();
  if (reason != HttpClientCloseReason::COMPLETE)
    diagnostics_.forcedClosure();
}

void BoundedHttpServer::acceptClient(uint32_t nowMillis) {
  if (core_.active()) {
    if (server_.hasClient()) {
      WiFiClient rejected = server_.accept();
      if (rejected) rejected.stop();
      diagnostics_.requestFailed();
      diagnostics_.forcedClosure();
    }
    return;
  }

  WiFiClient accepted = server_.accept();
  if (!accepted) return;
  if (!core_.beginConnection(nowMillis)) {
    accepted.stop();
    diagnostics_.requestFailed();
    diagnostics_.forcedClosure();
    return;
  }
  client_ = accepted;
  client_.setNoDelay(false);
#if defined(ARDUINO_ARCH_ESP8266)
  client_.setTimeout(ESP8266_WRITE_TIMEOUT_MS);
#endif
  diagnostics_.clientAccepted();
}

#if defined(ARDUINO_ARCH_ESP8266)
size_t BoundedHttpServer::writeAvailable(const uint8_t *data, size_t length,
                                         HttpBodyStorage storage) {
  if (!client_ || data == nullptr || length == 0) return 0;
  const int capacity = client_.availableForWrite();
  const size_t writable =
      HttpWritePlanner::plannedBytes(length, capacity, true);
  if (writable == 0) {
    diagnostics_.noCapacityTurn();
    return 0;
  }
  size_t written = 0;
  uint32_t started = 0;
  if (storage == HttpBodyStorage::PROGRAM_MEMORY) {
    memcpy_P(flashStaging_, data, writable);
    started = millis();
    written = client_.write(flashStaging_, writable);
  } else {
    started = millis();
    written = client_.write(data, writable);
  }
  diagnostics_.recordWrite(writable, written, millis() - started);
  return written;
}
#else
size_t BoundedHttpServer::writeAvailable(const uint8_t *data, size_t length,
                                         HttpBodyStorage storage,
                                         bool &disconnected) {
  disconnected = false;
  if (!client_ || data == nullptr || length == 0) return 0;
  (void)storage;
  const size_t writable = HttpWritePlanner::plannedBytes(length, 0, false);
  const uint32_t started = millis();
  const HttpSocketWriteResult result = HttpNonBlockingWriter::attempt(
      client_.fd(), data, writable, sendNonBlocking, nullptr);
  diagnostics_.recordWrite(writable, result.bytesWritten, millis() - started);
  disconnected = result.status == HttpSocketWriteStatus::DISCONNECTED ||
                 result.status == HttpSocketWriteStatus::ERROR;
  return result.bytesWritten;
}

#if defined(ARDUINO_ARCH_ESP32)
int BoundedHttpServer::sendNonBlocking(void *, int socketDescriptor,
                                       const uint8_t *data, size_t length,
                                       int &errorCode) {
  const int result = send(socketDescriptor, data, length, MSG_DONTWAIT);
  errorCode = result < 0 ? errno : 0;
  return result;
}
#endif  // ARDUINO_ARCH_ESP32
#endif  // ARDUINO_ARCH_ESP8266

void BoundedHttpServer::serviceClient(uint32_t nowMillis) {
  if (!core_.active()) return;
  if (core_.timedOut(nowMillis)) {
    diagnostics_.clientTimedOut(core_.responseReady());
    closeClient(HttpClientCloseReason::TIMEOUT, nowMillis);
    return;
  }
  if (!client_.connected() && client_.available() <= 0) {
    diagnostics_.requestFailed();
    closeClient(HttpClientCloseReason::DISCONNECT, nowMillis);
    return;
  }

  size_t processed = 0;
  while (processed < MAX_INPUT_BYTES_PER_UPDATE && client_.available() > 0) {
    const int value = client_.read();
    if (value < 0) break;
    core_.consume(static_cast<uint8_t>(value), nowMillis);
    ++processed;
  }

  if (!core_.responseReady()) return;
  size_t available = 0;
  const uint8_t *data = core_.outputData(available);
#if defined(ARDUINO_ARCH_ESP32)
  bool disconnected = false;
  const size_t written =
      writeAvailable(data, available, core_.outputStorage(), disconnected);
  if (disconnected) {
    diagnostics_.requestFailed();
    closeClient(HttpClientCloseReason::DISCONNECT, millis());
    return;
  }
#else
  const size_t written =
      writeAvailable(data, available, core_.outputStorage());
#endif
  core_.consumeOutput(written, nowMillis);
  if (core_.complete()) {
    if (core_.apOperationalTransitionResponse()) {
      apOperationalTransitionDelivered_ = true;
    }
    if (core_.wifiRestartResponse()) wifiRestartResponseDelivered_ = true;
    const uint32_t completedMillis = millis();
    diagnostics_.responseServed(core_.statusCode(),
                                completedMillis - core_.responseStartedMillis(),
                                core_.responseBytesSent());
    closeClient(HttpClientCloseReason::COMPLETE, completedMillis);
  }
}
#endif

void BoundedHttpServer::update(const HttpServiceGate::Decision &decision,
                               uint32_t nowMillis) {
  diagnostics_.observeGate(decision.allowed, decision.reason, nowMillis);
  const HttpLifecycleActions actions =
      lifecycle_.update(decision.allowed, core_.active(), nowMillis);
#if WSPR_HAS_WIFI
  if (actions.closeClient) closeClient(HttpClientCloseReason::GATE, nowMillis);
  if (actions.stopListener) stopListener();
  if (actions.startListener) {
    const IPAddress address =
        profile_ != HttpTransportCore::Profile::NORMAL
            ? WiFi.softAPIP() : WiFi.localIP();
    snprintf(localHost_, sizeof(localHost_), "%u.%u.%u.%u", address[0],
             address[1], address[2], address[3]);
    core_.setSecurityContext(localHost_, csrfToken_);
    startListener();
  }
  if (!actions.serviceClient) return;
  acceptClient(nowMillis);
  serviceClient(nowMillis);
#else
  (void)actions;
  (void)nowMillis;
#endif
}

#if !WSPR_HAS_WIFI
BoundedHttpServer::BoundedHttpServer() = default;
void BoundedHttpServer::beginSecurity() {}
bool BoundedHttpServer::transitionProfile(HttpTransportCore::Profile profile) {
  setProfile(profile);
  return true;
}
#endif
