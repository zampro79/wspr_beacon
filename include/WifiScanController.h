#pragma once

#include <stddef.h>
#include <stdint.h>

enum class WifiScanState : uint8_t { IDLE, SCANNING, READY, FAILED };
enum class WifiScanError : uint8_t {
  NONE, START_FAILED, TIMEOUT, DRIVER_FAILED, RECOVERY_EXHAUSTED,
  AP_RESTORE_FAILED
};
enum class WifiScanDriverStatus : int8_t { FAILED = -1, RUNNING = 0, COMPLETE = 1 };
enum class WifiScanStartResult : uint8_t {
  STARTED,
  ALREADY_SCANNING,
  DRIVER_BUSY,
  START_FAILED
};

struct WifiScanEntry {
  char ssid[33] = {};
  int32_t rssi = 0;
  bool encrypted = false;
};

struct WifiScanSnapshot {
  WifiScanState state = WifiScanState::IDLE;
  uint32_t generation = 0;
  uint32_t completedMillis = 0;
  uint8_t count = 0;
  bool completedTimestampValid = false;
  bool driverBusy = false;
  bool recoveryBlocked = false;
  bool apRestored = false;
  WifiScanError error = WifiScanError::NONE;
  WifiScanEntry entries[10] = {};
};

class WifiScanDriver {
 public:
  virtual ~WifiScanDriver() = default;
  virtual bool startAsync() = 0;
  virtual WifiScanDriverStatus poll(uint16_t &count) = 0;
  virtual bool read(uint16_t index, WifiScanEntry &entry) = 0;
  virtual void cleanup() = 0;
};

class WifiScanController {
 public:
  static constexpr uint8_t MAX_RESULTS = 10;
  static constexpr uint32_t TIMEOUT_MS = 10000UL;
  static constexpr uint32_t RECOVERY_TIMEOUT_MS = 5000UL;
  static constexpr uint8_t MAX_RECOVERY_POLLS = 32;
  static constexpr uint32_t STALE_MS = 60000UL;

  WifiScanStartResult start(WifiScanDriver &driver, uint32_t nowMillis);
  void update(WifiScanDriver &driver, uint32_t nowMillis);
  const WifiScanSnapshot &snapshot() const { return snapshot_; }
  bool stale(uint32_t nowMillis) const;
  uint32_t age(uint32_t nowMillis) const;
  void consume();
  void setApRestored(bool restored);
  void blockApRestore();

 private:
  WifiScanSnapshot snapshot_;
  uint32_t startedMillis_ = 0;
  bool logicalTimedOut_ = false;
  uint32_t recoveryStartedMillis_ = 0;
  uint8_t recoveryPolls_ = 0;

  void copyResults(WifiScanDriver &driver, uint16_t count);
  void insert(const WifiScanEntry &entry);
};
