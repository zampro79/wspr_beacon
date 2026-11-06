#pragma once

#include "ConfigurationService.h"
#include "WsprTransmitter.h"

class WsprConfigurationRuntime final : public ConfigurationRuntime {
 public:
  using PowerSaveApply = void (*)(bool enabled);

  explicit WsprConfigurationRuntime(WsprTransmitter &transmitter,
                                    PowerSaveApply powerSaveApply = nullptr)
      : transmitter_(transmitter), powerSaveApply_(powerSaveApply) {}

  bool busy() const override {
    return transmitter_.state() == TransmitterState::PREPARING ||
           transmitter_.state() == TransmitterState::TRANSMITTING;
  }
  bool applyWspr(const BeaconSettings &settings) override {
    return transmitter_.applySettings(settings);
  }
  void applyPowerSave(bool enabled) override {
    if (powerSaveApply_ != nullptr) powerSaveApply_(enabled);
  }
  bool selectFrequencyMode(WsprFrequencyMode mode) override {
    return transmitter_.setFrequencyMode(mode);
  }

 private:
  WsprTransmitter &transmitter_;
  PowerSaveApply powerSaveApply_;
};
