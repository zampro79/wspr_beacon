#pragma once

#include <Arduino.h>

#include "WifiConnectionTracker.h"
#include "WifiScanController.h"

class WifiManager {
 public:
  static constexpr uint32_t CONNECTION_TIMEOUT_MS = 30000UL;
  static constexpr uint32_t RECONNECT_DELAY_MS = 10000UL;
  static constexpr uint8_t MAX_SCAN_RESULTS = WifiScanController::MAX_RESULTS;

  void begin(bool enabled, const char *ssid, const char *password);
  void update(uint32_t monotonicMillis, bool allowNetworkWork = true);
  bool connect(const char *ssid, const char *password,
               uint32_t monotonicMillis);
  void disconnect(uint32_t monotonicMillis);
  bool startScan();
  WifiScanStartResult startProvisioningScan(uint32_t monotonicMillis);
  bool startProvisioningAp(const char *ssid, const char *password);
  bool connected() const;
  bool physicalLinkKnown() const { return physicalLinkKnown_; }
  bool physicalLinkConnected() const { return physicalLinkConnected_; }
  uint32_t lastPhysicalLinkTransitionMillis() const {
    return lastPhysicalLinkTransitionMillis_;
  }
  bool reconnectionPending() const {
    return reconnecting_ ||
           connection_.state() == WifiConnectionState::STATE_DISCONNECTED ||
           connection_.state() == WifiConnectionState::STATE_FAILED;
  }
  bool reconnecting() const { return reconnecting_; }
  void localIpv4(char destination[16]) const;
  bool enabled() const { return enabled_; }
  bool apActive() const { return apActive_; }
  WifiConnectionState state() const { return connection_.state(); }
  WifiScanState scanState() const { return scanController_.snapshot().state; }
  uint8_t scanCount() const { return scanController_.snapshot().count; }
  const WifiScanEntry &scanEntry(uint8_t index) const {
    return scanController_.snapshot().entries[index];
  }
  const WifiScanSnapshot &scanSnapshot() const {
    return scanController_.snapshot();
  }
  bool scanStale(uint32_t nowMillis) const {
    return scanController_.stale(nowMillis);
  }
  uint32_t scanAge(uint32_t nowMillis) const {
    return scanController_.age(nowMillis);
  }
  void consumeScanResults();
  bool apModeConfirmed() const;

 private:
  bool enabled_ = false;
  WifiConnectionTracker connection_{CONNECTION_TIMEOUT_MS};
  WifiScanController scanController_;
  bool reconnecting_ = false;
  bool physicalLinkKnown_ = false;
  bool physicalLinkConnected_ = false;
  bool apActive_ = false;
  uint32_t lastPhysicalLinkTransitionMillis_ = 0;
  void updateConnection(uint32_t monotonicMillis);
  void observePhysicalLink(bool connected, uint32_t monotonicMillis);
  void updateScan(uint32_t monotonicMillis);
};
