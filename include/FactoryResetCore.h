#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigStorage.h"

class ConfigurationManager;
class ManualTxController;
class WsprTransmitter;

enum class FactoryResetBootState : uint8_t {
  NORMAL,
  COMPLETED,
  RECOVERED,
  FAIL_CLOSED
};

enum class FactoryResetResult : uint8_t {
  COMPLETED,
  ALREADY_COMPLETED,
  STORAGE_FAILED,
  VERIFICATION_FAILED,
  RUNTIME_FAILED
};

class FactoryResetCore {
 public:
  static constexpr size_t MARKER_SIZE = 16;

  explicit FactoryResetCore(ConfigStorage &storage) : storage_(storage) {}

  FactoryResetBootState recover(ManualTxController &manualTx);
  FactoryResetResult reset(ManualTxController &manualTx,
                           ConfigurationManager &configuration,
                           WsprTransmitter &transmitter);
  bool factoryStateRequired() const {
    return bootState_ != FactoryResetBootState::NORMAL;
  }
  FactoryResetBootState bootState() const { return bootState_; }

 private:
  enum class MarkerState : uint8_t { NONE, IN_PROGRESS, COMPLETE, INVALID };
  struct MarkerSnapshot {
    MarkerState state = MarkerState::NONE;
    uint32_t sequence = 0;
    uint8_t slot = 0;
    bool storageError = false;
  };

  ConfigStorage &storage_;
  FactoryResetBootState bootState_ = FactoryResetBootState::NORMAL;

  MarkerSnapshot readMarker() const;
  bool writeMarker(MarkerState state, const MarkerSnapshot &previous,
                   MarkerSnapshot &written);
  FactoryResetResult finish(const MarkerSnapshot &pending);
  bool invalidateAndVerify(ConfigRecordType type, uint8_t slot);
};
