#pragma once

#include <Arduino.h>
#include <JTEncode.h>

#include "BandPlan.h"
#include "BeaconConfig.h"
#include "BeaconSettings.h"
#include "LocatorService.h"
#include "OperatingIdentity.h"
#include "RfSafetyController.h"
#include "TimeService.h"
#include "TxTelemetry.h"
#include "TxPreparedSnapshot.h"
#include "WsprScheduler.h"
#include "WsprFrequencySelector.h"

enum class TransmitterState : uint8_t {
  RF_INHIBITED,
  WAITING_FOR_TIME,
  IDLE,
  PREPARING,
  TRANSMITTING
};

class WsprTransmitter {
 public:
  WsprTransmitter(BeaconConfig &config, RfSafetyController &rf,
                  JTEncode &encoder);
  bool begin();
  bool recoverRf();
  bool applySettings(const BeaconSettings &settings);
  bool setOperatingIdentityUnconfigured(const BeaconSettings &settings);
  void manualTxOff();
  void manualTxOn();
  void reportCriticalRfError();
  void update(const TimeSnapshot &time, const LocatorSnapshot &location,
              uint32_t nowMillis, uint32_t nowMicros);
  bool releasePreparation();
  bool preparationMarginMillis(uint32_t nowMillis,
                               uint32_t &remainingMillis) const;
  bool timeAuthorized(const TimeSnapshot &time) const {
    return WsprScheduler::transmissionAuthorized(time);
  }
  WsprTimeAuthorization timeAuthorization(const TimeSnapshot &time) const {
    if (state_ == TransmitterState::PREPARING ||
        state_ == TransmitterState::TRANSMITTING) {
      return scheduler_.authorization();
    }
    return WsprScheduler::authorizationFor(time);
  }
  bool selectNextBand();
  bool setFrequencyMode(WsprFrequencyMode mode);
  void setTxAdmissionAllowed(bool allowed);
  bool txAdmissionAllowed() const {
    return txAdmissionAllowed_ && identityConfigured_;
  }
  OperatingIdentityState operatingIdentityState() const {
    return identityConfigured_
        ? OperatingIdentityState::CONFIGURED
        : OperatingIdentityState::OPERATING_IDENTITY_UNCONFIGURED;
  }
  bool configurationSessionSafe() const {
    return state_ != TransmitterState::PREPARING &&
           state_ != TransmitterState::TRANSMITTING;
  }
  bool rfOutputEnabled() const { return rf_.outputEnabled(); }
  void seedFrequencySelector(uint32_t seed) { frequencySelector_.seedOnce(seed); }
  WsprFrequencyMode frequencyMode() const { return config_.frequencyMode; }
  WsprBandId bandId() const { return config_.band; }
  const char *bandName() const;
  uint32_t frequencyHz() const { return transmitFrequencyHz_; }
  uint32_t configuredFrequencyHz() const {
    return static_cast<uint32_t>(static_cast<int64_t>(config_.baseFrequencyHz) +
                                 config_.frequencyOffsetHz);
  }
  WsprMessageSnapshot messageSnapshot(
      const LocatorSnapshot &location) const;
  TransmitterState state() const { return state_; }
  const TxTelemetrySnapshot &telemetry() const { return telemetry_.snapshot(); }
  const TxPreparedSnapshot &preparedSnapshot() const { return prepared_; }
  const char *stateLabel() const;

 private:
  static constexpr uint16_t SYMBOL_COUNT = WSPR_SYMBOL_COUNT;
  static_assert(SYMBOL_COUNT == WsprScheduler::SYMBOL_COUNT,
                "WSPR scheduler and encoder symbol counts must match");
  static constexpr uint16_t TONE_SPACING_CENTIHZ = 146;

  BeaconConfig &config_;
  RfSafetyController &rf_;
  JTEncode &encoder_;
  TransmitterState state_ = TransmitterState::WAITING_FOR_TIME;
  uint8_t symbols_[SYMBOL_COUNT] = {};
  WsprFrequencySelector frequencySelector_;
  uint32_t transmitFrequencyHz_ = 0;
  uint32_t activeSlot_ = UINT32_MAX;
  bool initialized_ = false;
  WsprScheduler scheduler_;
  TxTelemetry telemetry_;
  TxPreparedSnapshot prepared_;
  bool firstSymbolPrepared_ = false;
  bool txAdmissionAllowed_ = true;
  bool identityConfigured_ = false;

  bool prepareTransmission(const LocatorSnapshot &location);
  bool prepareFirstSymbol();
  bool startTransmission();
  void finishTransmission();
  void abortTransmission(TxInhibitReason reason);
};
