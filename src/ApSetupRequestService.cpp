#include "ApSetupRequestService.h"

#include "ApSetupRequestJson.h"
#include "WsprTransmitter.h"

ApSetupRequestResult ApSetupRequestService::execute(
    const char *body, size_t length, const char *sessionId,
    uint32_t nowMillis, uint32_t currentRevision, bool operationalMode,
    bool factoryResetPending, ConfigurationSessionController &sessions,
    WsprTransmitter &transmitter, ApSetupMarker &marker,
    WifiRestartController &restart) {
  if (!operationalMode) return ApSetupRequestResult::OPERATIONAL_MODE_REQUIRED;
  if (factoryResetPending) return ApSetupRequestResult::FACTORY_RESET_PENDING;
  if (restart.pending()) return ApSetupRequestResult::RESTART_PENDING;
  if (sessions.authorizeWebMutation(sessionId, nowMillis, transmitter) !=
      ConfigurationSessionResult::OK)
    return ApSetupRequestResult::INVALID_SESSION;
  ApSetupRequest request;
  if (ApSetupRequestJson::parse(body, length, request) != ApSetupParseResult::OK)
    return ApSetupRequestResult::INVALID_REQUEST;
  if (request.expectedRevision != currentRevision)
    return ApSetupRequestResult::STALE_REVISION;
  if (!transmitter.configurationSessionSafe() ||
      transmitter.txAdmissionAllowed() || transmitter.rfOutputEnabled())
    return ApSetupRequestResult::RF_UNSAFE;
  ConfigurationOperationGuard guard(sessions, transmitter);
  if (!guard.active()) return ApSetupRequestResult::OPERATION_BUSY;
  const ApSetupMarkerResult markerResult = marker.request();
  if (markerResult == ApSetupMarkerResult::ALREADY_PENDING)
    return ApSetupRequestResult::ALREADY_PENDING;
  if (markerResult != ApSetupMarkerResult::OK)
    return markerResult == ApSetupMarkerResult::VERIFICATION_FAILED
        ? ApSetupRequestResult::MARKER_VERIFICATION_FAILED
        : ApSetupRequestResult::MARKER_WRITE_FAILED;
  if (!restart.schedule(nowMillis)) return ApSetupRequestResult::RESTART_PENDING;
  return ApSetupRequestResult::ACCEPTED;
}
