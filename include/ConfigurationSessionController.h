#pragma once

#include <stdint.h>

#include "WsprTransmitter.h"

enum class FirmwareOperatingMode : uint8_t { NORMAL, CONFIGURATION };

enum class ConfigurationSessionResult : uint8_t {
  OK,
  UNCHANGED,
  INVALID_IDENTIFIER,
  RF_BUSY,
  OWNER_MISMATCH,
  NOT_ACTIVE
};

struct ConfigurationSessionSnapshot {
  FirmwareOperatingMode mode = FirmwareOperatingMode::NORMAL;
  uint32_t revision = 0;
  uint32_t webRemainingMillis = 0;
  bool cliActive = false;
  bool webActive = false;
  bool resumeGuardActive = false;
  bool operationActive = false;
  bool txAdmissionAllowed = true;
};

class ConfigurationSessionController {
 public:
  static constexpr uint32_t WEB_HEARTBEAT_INTERVAL_MS = 5000UL;
  static constexpr uint32_t WEB_SESSION_EXPIRY_MS = 25000UL;
  static constexpr uint32_t RESUME_SLOT_GUARD_MS = 7000UL;
  static constexpr size_t WEB_SESSION_ID_HEX_LENGTH = 32;

  ConfigurationSessionResult acquireCli(WsprTransmitter &transmitter);
  ConfigurationSessionResult releaseCli(WsprTransmitter &transmitter);
  ConfigurationSessionResult acquireWeb(const char *sessionId,
                                        uint32_t nowMillis,
                                        WsprTransmitter &transmitter);
  ConfigurationSessionResult renewWeb(const char *sessionId,
                                      uint32_t nowMillis,
                                      WsprTransmitter &transmitter);
  ConfigurationSessionResult releaseWeb(const char *sessionId,
                                        WsprTransmitter &transmitter);
  ConfigurationSessionResult authorizeWebMutation(
      const char *sessionId, uint32_t nowMillis,
      WsprTransmitter &transmitter);
  bool expireWeb(uint32_t nowMillis, WsprTransmitter &transmitter);
  void invalidateAll(WsprTransmitter &transmitter);

  bool beginOperation(WsprTransmitter &transmitter);
  void endOperation(WsprTransmitter &transmitter);
  void activateResumeGuard(WsprTransmitter &transmitter);

  void updateTxAdmission(bool timeQualified, bool rfReady,
                         bool nextSlotKnown, uint32_t millisUntilSlot,
                         WsprTransmitter &transmitter);

  ConfigurationSessionSnapshot snapshot(uint32_t nowMillis) const;
  FirmwareOperatingMode mode() const {
    return cliActive_ || webActive_ ? FirmwareOperatingMode::CONFIGURATION
                                    : FirmwareOperatingMode::NORMAL;
  }
  bool cliActive() const { return cliActive_; }
  bool webActive() const { return webActive_; }
  bool resumeGuardActive() const { return resumeGuardActive_; }
  bool operationActive() const { return operationActive_; }
  uint32_t revision() const { return revision_; }
  bool txAdmissionAllowed() const {
    return mode() == FirmwareOperatingMode::NORMAL &&
           !resumeGuardActive_ && !operationActive_;
  }

  static bool validWebSessionId(const char *sessionId);
  static uint32_t nextRevision(uint32_t revision) { return revision + 1U; }

 private:
  char webSessionId_[WEB_SESSION_ID_HEX_LENGTH + 1U] = {};
  uint32_t webHeartbeatMillis_ = 0;
  uint32_t revision_ = 0;
  bool cliActive_ = false;
  bool webActive_ = false;
  bool resumeGuardActive_ = false;
  bool operationActive_ = false;

  bool acquisitionSafe(const WsprTransmitter &transmitter) const;
  bool webOwnerMatches(const char *sessionId) const;
  void ownerChanged() { revision_ = nextRevision(revision_); }
  void finalOwnerReleased(WsprTransmitter &transmitter);
};

class ConfigurationOperationGuard {
 public:
  ConfigurationOperationGuard(ConfigurationSessionController &controller,
                              WsprTransmitter &transmitter)
      : controller_(controller), transmitter_(transmitter),
        active_(controller_.beginOperation(transmitter_)) {}
  ~ConfigurationOperationGuard() {
    if (active_) controller_.endOperation(transmitter_);
  }

  bool active() const { return active_; }

  ConfigurationOperationGuard(const ConfigurationOperationGuard &) = delete;
  ConfigurationOperationGuard &operator=(
      const ConfigurationOperationGuard &) = delete;

 private:
  ConfigurationSessionController &controller_;
  WsprTransmitter &transmitter_;
  bool active_;
};
