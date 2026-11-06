#include "WifiManager.h"

#include <stdio.h>
#include <string.h>

#include "BoardConfig.h"
#include "DiagnosticRegistry.h"

#if WSPR_HAS_WIFI
  #if defined(ARDUINO_ARCH_ESP8266)
    #include <ESP8266WiFi.h>
  #else
    #include <WiFi.h>
  #endif
#endif

#if WSPR_HAS_WIFI
namespace {
class ArduinoWifiScanDriver : public WifiScanDriver {
 public:
  bool startAsync() override {
    return WiFi.scanNetworks(true, false) == WIFI_SCAN_RUNNING;
  }
  WifiScanDriverStatus poll(uint16_t &count) override {
    const int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) return WifiScanDriverStatus::RUNNING;
    if (found < 0) return WifiScanDriverStatus::FAILED;
    count = static_cast<uint16_t>(found);
    return WifiScanDriverStatus::COMPLETE;
  }
  bool read(uint16_t index, WifiScanEntry &entry) override {
    const String ssid = WiFi.SSID(index);
    if (ssid.length() == 0U || ssid.length() >= sizeof(entry.ssid)) return false;
    ssid.toCharArray(entry.ssid, sizeof(entry.ssid));
    entry.rssi = WiFi.RSSI(index);
#if defined(ARDUINO_ARCH_ESP8266)
    entry.encrypted = WiFi.encryptionType(index) != ENC_TYPE_NONE;
#else
    entry.encrypted = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
#endif
    return true;
  }
  void cleanup() override { WiFi.scanDelete(); }
};
ArduinoWifiScanDriver scanDriver;
}
#endif

void WifiManager::begin(bool enabled, const char *ssid, const char *password) {
  apActive_ = false;
  physicalLinkKnown_ = false;
  physicalLinkConnected_ = false;
  lastPhysicalLinkTransitionMillis_ = millis();
  if (enabled && ssid != nullptr && ssid[0] != '\0') {
    connect(ssid, password, millis());
    return;
  }
  enabled_ = false;
  connection_.disable();
#if WSPR_HAS_WIFI
  WiFi.mode(WIFI_OFF);
#else
  (void)ssid;
  (void)password;
#endif
  diagnostics.setState(DiagnosticModule::WIFI, DiagnosticState::STATE_DISABLED,
                       DiagnosticReason::DISABLED_BY_CONFIGURATION, millis());
}

bool WifiManager::startProvisioningAp(const char *ssid, const char *password) {
#if WSPR_HAS_WIFI
  if (ssid == nullptr || ssid[0] == '\0' || password == nullptr ||
      strlen(password) != 8U) return false;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.setAutoConnect(false);
#endif
  WiFi.disconnect(false);
  if (!WiFi.mode(WIFI_AP)) return false;
  const IPAddress address(192, 168, 4, 1);
  const IPAddress netmask(255, 255, 255, 0);
  if (!WiFi.softAPConfig(address, address, netmask) ||
      !WiFi.softAP(ssid, password, 1, false, 1)) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
  enabled_ = false;
  reconnecting_ = false;
  connection_.disable();
  apActive_ = true;
  diagnostics.setState(DiagnosticModule::WIFI,
                       DiagnosticState::STATE_OPERATIONAL,
                       DiagnosticReason::NONE, millis());
  return true;
#else
  (void)ssid;
  (void)password;
  return false;
#endif
}

void WifiManager::update(uint32_t monotonicMillis, bool allowNetworkWork) {
  if (!allowNetworkWork) return;
  updateConnection(monotonicMillis);
  updateScan(monotonicMillis);
}

bool WifiManager::connect(const char *ssid, const char *password,
                          uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  if (ssid == nullptr || ssid[0] == '\0' || password == nullptr) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password[0] == '\0' ? nullptr : password);
  enabled_ = true;
  reconnecting_ = false;
  connection_.beginConnecting(monotonicMillis);
  diagnostics.setState(DiagnosticModule::WIFI,
                       DiagnosticState::STATE_INITIALIZING,
                       DiagnosticReason::WAITING_FOR_DATA, monotonicMillis);
  return true;
#else
  (void)ssid;
  (void)password;
  (void)monotonicMillis;
  return false;
#endif
}

void WifiManager::disconnect(uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
#endif
  enabled_ = false;
  apActive_ = false;
  reconnecting_ = false;
  observePhysicalLink(false, monotonicMillis);
  connection_.disable();
  diagnostics.setState(DiagnosticModule::WIFI, DiagnosticState::STATE_DISABLED,
                       DiagnosticReason::DISABLED_BY_CONFIGURATION,
                       monotonicMillis);
}

void WifiManager::updateConnection(uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  if (!enabled_) return;
  const bool nowConnected = WiFi.status() == WL_CONNECTED;
  observePhysicalLink(nowConnected, monotonicMillis);
  const WifiConnectionState previous = connection_.state();
  const WifiConnectionState current =
      connection_.update(monotonicMillis, nowConnected);
  if (current != previous) {
    if (current == WifiConnectionState::STATE_CONNECTED) {
      reconnecting_ = false;
      diagnostics.recordSuccess(DiagnosticModule::WIFI);
      diagnostics.setState(DiagnosticModule::WIFI,
                           DiagnosticState::STATE_OPERATIONAL,
                           DiagnosticReason::NONE, monotonicMillis);
    } else if (current == WifiConnectionState::STATE_DISCONNECTED) {
      diagnostics.recordFailure(DiagnosticModule::WIFI);
      diagnostics.setState(DiagnosticModule::WIFI,
                           DiagnosticState::STATE_DEGRADED,
                           DiagnosticReason::SERVICE_UNAVAILABLE,
                           monotonicMillis);
    } else if (current == WifiConnectionState::STATE_FAILED) {
      diagnostics.recordFailure(DiagnosticModule::WIFI);
      diagnostics.setState(DiagnosticModule::WIFI,
                           DiagnosticState::STATE_DEGRADED,
                           DiagnosticReason::TIMEOUT, monotonicMillis);
    }
  }
  if (!scanController_.snapshot().driverBusy &&
      connection_.retryDue(monotonicMillis, RECONNECT_DELAY_MS)) {
    WiFi.reconnect();
    reconnecting_ = true;
    connection_.beginConnecting(monotonicMillis);
    diagnostics.setState(DiagnosticModule::WIFI,
                         DiagnosticState::STATE_INITIALIZING,
                         DiagnosticReason::WAITING_FOR_DATA,
                         monotonicMillis);
  }
#else
  (void)monotonicMillis;
#endif
}

void WifiManager::observePhysicalLink(bool connected,
                                      uint32_t monotonicMillis) {
  if (!physicalLinkKnown_ || physicalLinkConnected_ != connected) {
    physicalLinkKnown_ = true;
    physicalLinkConnected_ = connected;
    lastPhysicalLinkTransitionMillis_ = monotonicMillis;
  }
}

bool WifiManager::connected() const {
  return enabled_ && connection_.connected();
}

void WifiManager::localIpv4(char destination[16]) const {
  destination[0] = '\0';
#if WSPR_HAS_WIFI
  if (!connected() && !apActive_) return;
  const IPAddress address = apActive_ ? WiFi.softAPIP() : WiFi.localIP();
  snprintf(destination, 16, "%u.%u.%u.%u", address[0], address[1],
           address[2], address[3]);
#endif
}

bool WifiManager::startScan() {
#if WSPR_HAS_WIFI
  if (apActive_) {
    return startProvisioningScan(millis()) == WifiScanStartResult::STARTED;
  }
  WiFi.mode(WIFI_STA);
  return scanController_.start(scanDriver, millis()) ==
         WifiScanStartResult::STARTED;
#else
  return false;
#endif
}

WifiScanStartResult WifiManager::startProvisioningScan(
    uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  if (!apActive_) return WifiScanStartResult::START_FAILED;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
#if defined(ARDUINO_ARCH_ESP8266)
  WiFi.setAutoConnect(false);
#endif
  WiFi.disconnect(false);
  if (!WiFi.mode(WIFI_AP_STA)) return WifiScanStartResult::START_FAILED;
  return scanController_.start(scanDriver, monotonicMillis);
#else
  (void)monotonicMillis;
  return WifiScanStartResult::START_FAILED;
#endif
}

void WifiManager::updateScan(uint32_t monotonicMillis) {
#if WSPR_HAS_WIFI
  scanController_.update(scanDriver, monotonicMillis);
#else
  (void)monotonicMillis;
#endif
}

bool WifiManager::apModeConfirmed() const {
#if WSPR_HAS_WIFI
  return apActive_ && (WiFi.getMode() & WIFI_AP) != 0;
#else
  return false;
#endif
}


void WifiManager::consumeScanResults() {
  scanController_.consume();
}
