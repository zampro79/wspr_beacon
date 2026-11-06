#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ApSetupMarker.h"
#include "ConfigurationSessionController.h"
#include "WifiRestartController.h"

enum class ApSetupRequestResult : uint8_t {
  ACCEPTED, OPERATIONAL_MODE_REQUIRED, FACTORY_RESET_PENDING,
  RESTART_PENDING, INVALID_SESSION, INVALID_REQUEST, STALE_REVISION,
  RF_UNSAFE, OPERATION_BUSY, ALREADY_PENDING, MARKER_WRITE_FAILED,
  MARKER_VERIFICATION_FAILED
};

class ApSetupRequestService {
 public:
  static ApSetupRequestResult execute(
      const char *body, size_t length, const char *sessionId,
      uint32_t nowMillis, uint32_t currentRevision, bool operationalMode,
      bool factoryResetPending, ConfigurationSessionController &sessions,
      WsprTransmitter &transmitter, ApSetupMarker &marker,
      WifiRestartController &restart);
};
