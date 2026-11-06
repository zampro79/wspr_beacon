#include "ConfigurationSessionController.h"

#include <string.h>

namespace {
bool constantTimeEqual(const char *left, const char *right) {
  if (left == nullptr || right == nullptr) return false;
  unsigned difference = 0;
  for (size_t index = 0;
       index < ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH;
       ++index) {
    difference |= static_cast<unsigned char>(left[index]) ^
                  static_cast<unsigned char>(right[index]);
  }
  return difference == 0U &&
         left[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH] ==
             '\0' &&
         right[ConfigurationSessionController::WEB_SESSION_ID_HEX_LENGTH] ==
             '\0';
}
}  // namespace

bool ConfigurationSessionController::validWebSessionId(
    const char *sessionId) {
  if (sessionId == nullptr) return false;
  for (size_t index = 0; index < WEB_SESSION_ID_HEX_LENGTH; ++index) {
    const char value = sessionId[index];
    if (!((value >= '0' && value <= '9') ||
          (value >= 'a' && value <= 'f'))) {
      return false;
    }
  }
  return sessionId[WEB_SESSION_ID_HEX_LENGTH] == '\0';
}

bool ConfigurationSessionController::acquisitionSafe(
    const WsprTransmitter &transmitter) const {
  return transmitter.configurationSessionSafe() &&
         !transmitter.rfOutputEnabled();
}

bool ConfigurationSessionController::webOwnerMatches(
    const char *sessionId) const {
  return webActive_ && validWebSessionId(sessionId) &&
         constantTimeEqual(webSessionId_, sessionId);
}

ConfigurationSessionResult ConfigurationSessionController::acquireCli(
    WsprTransmitter &transmitter) {
  if (cliActive_) return ConfigurationSessionResult::UNCHANGED;
  if (!acquisitionSafe(transmitter)) return ConfigurationSessionResult::RF_BUSY;
  transmitter.setTxAdmissionAllowed(false);
  cliActive_ = true;
  ownerChanged();
  return ConfigurationSessionResult::OK;
}

ConfigurationSessionResult ConfigurationSessionController::releaseCli(
    WsprTransmitter &transmitter) {
  if (!cliActive_) return ConfigurationSessionResult::UNCHANGED;
  cliActive_ = false;
  ownerChanged();
  if (!webActive_) finalOwnerReleased(transmitter);
  return ConfigurationSessionResult::OK;
}

ConfigurationSessionResult ConfigurationSessionController::acquireWeb(
    const char *sessionId, uint32_t nowMillis,
    WsprTransmitter &transmitter) {
  if (!validWebSessionId(sessionId)) {
    return ConfigurationSessionResult::INVALID_IDENTIFIER;
  }
  expireWeb(nowMillis, transmitter);
  if (webActive_) {
    if (!webOwnerMatches(sessionId)) {
      return ConfigurationSessionResult::OWNER_MISMATCH;
    }
    webHeartbeatMillis_ = nowMillis;
    return ConfigurationSessionResult::UNCHANGED;
  }
  if (!acquisitionSafe(transmitter)) return ConfigurationSessionResult::RF_BUSY;
  transmitter.setTxAdmissionAllowed(false);
  memcpy(webSessionId_, sessionId, sizeof(webSessionId_));
  webHeartbeatMillis_ = nowMillis;
  webActive_ = true;
  ownerChanged();
  return ConfigurationSessionResult::OK;
}

ConfigurationSessionResult ConfigurationSessionController::renewWeb(
    const char *sessionId, uint32_t nowMillis,
    WsprTransmitter &transmitter) {
  if (webActive_ &&
      static_cast<uint32_t>(nowMillis - webHeartbeatMillis_) >=
          WEB_SESSION_EXPIRY_MS) {
    expireWeb(nowMillis, transmitter);
    return ConfigurationSessionResult::NOT_ACTIVE;
  }
  if (!webActive_) return ConfigurationSessionResult::NOT_ACTIVE;
  if (!webOwnerMatches(sessionId)) {
    return ConfigurationSessionResult::OWNER_MISMATCH;
  }
  webHeartbeatMillis_ = nowMillis;
  return ConfigurationSessionResult::OK;
}

ConfigurationSessionResult ConfigurationSessionController::releaseWeb(
    const char *sessionId, WsprTransmitter &transmitter) {
  if (!webActive_) return ConfigurationSessionResult::NOT_ACTIVE;
  if (!webOwnerMatches(sessionId)) {
    return ConfigurationSessionResult::OWNER_MISMATCH;
  }
  webActive_ = false;
  memset(webSessionId_, 0, sizeof(webSessionId_));
  ownerChanged();
  if (!cliActive_) finalOwnerReleased(transmitter);
  return ConfigurationSessionResult::OK;
}

ConfigurationSessionResult ConfigurationSessionController::authorizeWebMutation(
    const char *sessionId, uint32_t nowMillis,
    WsprTransmitter &transmitter) {
  expireWeb(nowMillis, transmitter);
  if (!webActive_) return ConfigurationSessionResult::NOT_ACTIVE;
  return webOwnerMatches(sessionId) ? ConfigurationSessionResult::OK
                                    : ConfigurationSessionResult::OWNER_MISMATCH;
}

bool ConfigurationSessionController::expireWeb(
    uint32_t nowMillis, WsprTransmitter &transmitter) {
  if (!webActive_ ||
      static_cast<uint32_t>(nowMillis - webHeartbeatMillis_) <
          WEB_SESSION_EXPIRY_MS) {
    return false;
  }
  webActive_ = false;
  memset(webSessionId_, 0, sizeof(webSessionId_));
  ownerChanged();
  if (!cliActive_) finalOwnerReleased(transmitter);
  return true;
}

void ConfigurationSessionController::invalidateAll(
    WsprTransmitter &transmitter) {
  cliActive_ = false;
  webActive_ = false;
  operationActive_ = false;
  memset(webSessionId_, 0, sizeof(webSessionId_));
  ownerChanged();
  activateResumeGuard(transmitter);
}

void ConfigurationSessionController::finalOwnerReleased(
    WsprTransmitter &transmitter) {
  activateResumeGuard(transmitter);
}

void ConfigurationSessionController::activateResumeGuard(
    WsprTransmitter &transmitter) {
  if (!resumeGuardActive_) {
    resumeGuardActive_ = true;
  }
  transmitter.setTxAdmissionAllowed(false);
}

bool ConfigurationSessionController::beginOperation(
    WsprTransmitter &transmitter) {
  if (mode() != FirmwareOperatingMode::CONFIGURATION || operationActive_) {
    return false;
  }
  transmitter.setTxAdmissionAllowed(false);
  operationActive_ = true;
  return true;
}

void ConfigurationSessionController::endOperation(
    WsprTransmitter &transmitter) {
  if (!operationActive_) return;
  operationActive_ = false;
  if (mode() == FirmwareOperatingMode::NORMAL) {
    resumeGuardActive_ = true;
  }
  transmitter.setTxAdmissionAllowed(txAdmissionAllowed());
}

void ConfigurationSessionController::updateTxAdmission(
    bool timeQualified, bool rfReady, bool nextSlotKnown,
    uint32_t millisUntilSlot, WsprTransmitter &transmitter) {
  if (resumeGuardActive_ && mode() == FirmwareOperatingMode::NORMAL &&
      !operationActive_ && timeQualified && rfReady && nextSlotKnown &&
      millisUntilSlot > RESUME_SLOT_GUARD_MS) {
    resumeGuardActive_ = false;
    ownerChanged();
  }
  transmitter.setTxAdmissionAllowed(txAdmissionAllowed());
}

ConfigurationSessionSnapshot ConfigurationSessionController::snapshot(
    uint32_t nowMillis) const {
  ConfigurationSessionSnapshot result;
  result.mode = mode();
  result.revision = revision_;
  result.cliActive = cliActive_;
  result.webActive = webActive_;
  result.resumeGuardActive = resumeGuardActive_;
  result.operationActive = operationActive_;
  result.txAdmissionAllowed = txAdmissionAllowed();
  if (webActive_) {
    const uint32_t elapsed =
        static_cast<uint32_t>(nowMillis - webHeartbeatMillis_);
    result.webRemainingMillis = elapsed >= WEB_SESSION_EXPIRY_MS
        ? 0 : WEB_SESSION_EXPIRY_MS - elapsed;
  }
  return result;
}
