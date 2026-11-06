#pragma once

#include <stdint.h>

#include "WsprScheduler.h"

enum class SystemMode : uint8_t {
  BOOT_DECISION,
  STA_CONNECTING_INITIAL,
  NORMAL,
  PROVISIONING_AP,
  AP_OPERATIONAL,
  PROVISIONING_ERROR
};

enum class ApOperationalRequestResult : uint8_t {
  ACCEPTED,
  ALREADY_PENDING,
  REJECTED
};

class BootModeController {
 public:
  static constexpr uint32_t INITIAL_STA_TIMEOUT_MS = 15000UL;

  void begin(bool usableSavedCredentials, uint32_t nowMillis);
  void update(bool staConnected, bool staFailed, uint32_t nowMillis);
  void apStartResult(bool succeeded);
  ApOperationalRequestResult requestApOperational();
  bool completeApOperationalTransition();
  void failApOperationalTransition();

  SystemMode mode() const { return mode_; }
  bool enterApRequested() const { return enterApRequested_; }
  void acknowledgeApRequest() { enterApRequested_ = false; }
  bool normal() const { return mode_ == SystemMode::NORMAL; }
  bool rfOperationAllowed() const {
    return mode_ == SystemMode::NORMAL || mode_ == SystemMode::AP_OPERATIONAL;
  }
  bool timeAuthorizationAllowed(WsprTimeAuthorization authorization) const {
    if (!rfOperationAllowed()) return false;
    return normal() ? authorization != WsprTimeAuthorization::NONE
                    : authorization == WsprTimeAuthorization::GPS_PRECISE;
  }
  bool httpNetworkAvailable(bool staConnected, bool apActive) const {
    return normal() ? staConnected
                    : (provisioning() || apOperational()) && apActive;
  }
  bool provisioning() const { return mode_ == SystemMode::PROVISIONING_AP; }
  bool apOperational() const { return mode_ == SystemMode::AP_OPERATIONAL; }
  bool apOperationalPending() const { return apOperationalPending_; }
  bool failClosed() const { return mode_ == SystemMode::PROVISIONING_ERROR; }

 private:
  SystemMode mode_ = SystemMode::BOOT_DECISION;
  uint32_t staStartedMillis_ = 0;
  bool enterApRequested_ = false;
  bool apOperationalPending_ = false;

  void requestAp();
};
