#pragma once

#include <stdint.h>

#include "FactoryResetCore.h"
#include "ConfigurationSessionController.h"

class ConfigurationManager;
class ManualTxController;
class WsprTransmitter;

enum class FactoryResetOwner : uint8_t { CLI, WEB };
enum class FactoryResetServiceResult : uint8_t {
  COMPLETED, ALREADY_COMPLETED, SESSION_REQUIRED, OWNER_CONFLICT,
  STALE_REVISION, OPERATION_BUSY, STORAGE_FAILED, VERIFICATION_FAILED,
  RUNTIME_FAILED
};

class FactoryResetService {
 public:
  FactoryResetService(FactoryResetCore &core, ConfigurationManager &configuration,
                      ConfigurationSessionController &sessions,
                      ManualTxController &manualTx,
                      WsprTransmitter &transmitter)
      : core_(core), configuration_(configuration), sessions_(sessions),
        manualTx_(manualTx), transmitter_(transmitter) {}

  FactoryResetServiceResult execute(FactoryResetOwner owner,
                                    const char *webSessionId,
                                    uint32_t expectedSessionRevision,
                                    uint32_t nowMillis);
  bool resetCompletedThisBoot() const { return resetCompletedThisBoot_; }

 private:
  FactoryResetCore &core_;
  ConfigurationManager &configuration_;
  ConfigurationSessionController &sessions_;
  ManualTxController &manualTx_;
  WsprTransmitter &transmitter_;
  bool resetCompletedThisBoot_ = false;
};
