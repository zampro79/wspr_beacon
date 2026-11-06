#include "FactoryResetService.h"

#include "ConfigurationManager.h"
#include "ManualTxController.h"
#include "WsprTransmitter.h"
#include "BandPlan.h"
#include <string.h>

FactoryResetServiceResult FactoryResetService::execute(
    FactoryResetOwner owner, const char *webSessionId,
    uint32_t expectedSessionRevision, uint32_t nowMillis) {
  if (resetCompletedThisBoot_) return FactoryResetServiceResult::ALREADY_COMPLETED;
  if (expectedSessionRevision != sessions_.revision())
    return FactoryResetServiceResult::STALE_REVISION;
  if (owner == FactoryResetOwner::CLI) {
    if (!sessions_.cliActive()) return FactoryResetServiceResult::SESSION_REQUIRED;
    if (sessions_.webActive()) return FactoryResetServiceResult::OWNER_CONFLICT;
  } else {
    if (sessions_.authorizeWebMutation(webSessionId, nowMillis, transmitter_) !=
        ConfigurationSessionResult::OK)
      return FactoryResetServiceResult::SESSION_REQUIRED;
    if (sessions_.cliActive()) return FactoryResetServiceResult::OWNER_CONFLICT;
  }
  ConfigurationOperationGuard operation(sessions_, transmitter_);
  if (!operation.active()) return FactoryResetServiceResult::OPERATION_BUSY;

  manualTx_.turnOff();
  const FactoryResetResult result =
      core_.reset(manualTx_, configuration_, transmitter_);
  sessions_.invalidateAll(transmitter_);
  const bool coreCompleted = result == FactoryResetResult::COMPLETED ||
                             result == FactoryResetResult::ALREADY_COMPLETED;
  const BeaconSettings &active = configuration_.activeWspr();
  const bool verified = coreCompleted && !manualTx_.permitted() &&
      active.callsign[0] == '\0' &&
      strcmp(active.fixedLocator, WSPR_DEFAULT_LOCATOR) == 0 &&
      active.band == WsprBandId::BAND_20M &&
      active.baseFrequencyHz == BandPlan::DEFAULT_NOMINAL_TONE_ZERO_HZ &&
      !configuration_.activeWifi().hasSsid() &&
      !configuration_.savedWifi().hasSsid();
  resetCompletedThisBoot_ = verified;
  if (coreCompleted && !verified) return FactoryResetServiceResult::RUNTIME_FAILED;
  switch (result) {
    case FactoryResetResult::COMPLETED: return FactoryResetServiceResult::COMPLETED;
    case FactoryResetResult::ALREADY_COMPLETED: return FactoryResetServiceResult::ALREADY_COMPLETED;
    case FactoryResetResult::STORAGE_FAILED: return FactoryResetServiceResult::STORAGE_FAILED;
    case FactoryResetResult::VERIFICATION_FAILED: return FactoryResetServiceResult::VERIFICATION_FAILED;
    case FactoryResetResult::RUNTIME_FAILED: return FactoryResetServiceResult::RUNTIME_FAILED;
  }
  return FactoryResetServiceResult::RUNTIME_FAILED;
}
