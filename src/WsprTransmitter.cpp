#include "WsprTransmitter.h"

#include <string.h>

#include "ConfigValidator.h"
#include "WsprConfigAdapter.h"

WsprTransmitter::WsprTransmitter(
    BeaconConfig &config, RfSafetyController &rf, JTEncode &encoder)
    : config_(config), rf_(rf), encoder_(encoder),
      identityConfigured_(config.callsign[0] != '\0') {
  transmitFrequencyHz_ = static_cast<uint32_t>(
      static_cast<int64_t>(config_.baseFrequencyHz) +
      config_.frequencyOffsetHz);
}

bool WsprTransmitter::begin() {
  initialized_ = true;
  const bool initialized = rf_.begin();
  if (!identityConfigured_) {
    rf_.disableOutput();
    state_ = TransmitterState::RF_INHIBITED;
    telemetry_.setInhibitReason(
        TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED);
    return initialized;
  }
  state_ = initialized ? TransmitterState::WAITING_FOR_TIME
                       : TransmitterState::RF_INHIBITED;
  telemetry_.setInhibitReason(initialized ? TxInhibitReason::TIME_UNAVAILABLE
                                          : TxInhibitReason::RF_UNAVAILABLE);
  return initialized;
}

bool WsprTransmitter::applySettings(const BeaconSettings &settings) {
  const bool busy = state_ == TransmitterState::PREPARING ||
                    state_ == TransmitterState::TRANSMITTING;
  if (!WsprConfigAdapter::apply(settings, busy, config_) ||
      !rf_.configure(config_.si5351ReferenceHz,
                     config_.si5351Correction)) {
    return false;
  }
  identityConfigured_ = true;
  transmitFrequencyHz_ = static_cast<uint32_t>(
      static_cast<int64_t>(config_.baseFrequencyHz) +
      config_.frequencyOffsetHz);
  if (initialized_) recoverRf();
  return true;
}

bool WsprTransmitter::setOperatingIdentityUnconfigured(
    const BeaconSettings &settings) {
  BeaconSettings validated = settings;
  strcpy(validated.callsign, "AA0AA");
  if (operatingIdentityConfigured(settings) ||
      ConfigValidator::validate(validated) != ConfigValidation::VALID ||
      state_ == TransmitterState::PREPARING ||
      state_ == TransmitterState::TRANSMITTING) {
    return false;
  }
  config_.callsign[0] = '\0';
  strncpy(config_.fixedLocator, settings.fixedLocator,
          sizeof(config_.fixedLocator));
  config_.fixedLocator[sizeof(config_.fixedLocator) - 1U] = '\0';
  config_.powerDbm = settings.powerDbm;
  config_.transmitIntervalMinutes = settings.transmitIntervalMinutes;
  config_.baseFrequencyHz = settings.baseFrequencyHz;
  config_.frequencyOffsetHz = settings.frequencyOffsetHz;
  config_.si5351Correction = settings.si5351Correction;
  config_.band = settings.band;
  config_.frequencyMode = settings.frequencyMode;
  transmitFrequencyHz_ = static_cast<uint32_t>(
      static_cast<int64_t>(config_.baseFrequencyHz) +
      config_.frequencyOffsetHz);
  identityConfigured_ = false;
  rf_.disableOutput();
  scheduler_.disarm();
  prepared_ = TxPreparedSnapshot();
  firstSymbolPrepared_ = false;
  state_ = TransmitterState::RF_INHIBITED;
  telemetry_.setInhibitReason(
      TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED);
  return true;
}

void WsprTransmitter::manualTxOff() {
  txAdmissionAllowed_ = false;
  abortTransmission(TxInhibitReason::MANUAL_TX_OFF);
}

void WsprTransmitter::manualTxOn() {
  if (!identityConfigured_) {
    telemetry_.setInhibitReason(
        TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED);
    return;
  }
  if (rf_.operational()) {
    state_ = TransmitterState::WAITING_FOR_TIME;
    telemetry_.setInhibitReason(TxInhibitReason::TIME_UNAVAILABLE);
  } else {
    state_ = TransmitterState::RF_INHIBITED;
    telemetry_.setInhibitReason(TxInhibitReason::RF_UNAVAILABLE);
  }
}

bool WsprTransmitter::recoverRf() {
  if (state_ == TransmitterState::PREPARING ||
      state_ == TransmitterState::TRANSMITTING) {
    return false;
  }
  const bool recovered = rf_.recover();
  if (!identityConfigured_) {
    rf_.disableOutput();
    state_ = TransmitterState::RF_INHIBITED;
    telemetry_.setInhibitReason(
        TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED);
    return recovered;
  }
  state_ = recovered ? TransmitterState::WAITING_FOR_TIME
                     : TransmitterState::RF_INHIBITED;
  telemetry_.setInhibitReason(recovered ? TxInhibitReason::TIME_UNAVAILABLE
                                        : TxInhibitReason::RF_UNAVAILABLE);
  return recovered;
}

void WsprTransmitter::reportCriticalRfError() {
  rf_.reportCriticalError();
  abortTransmission(TxInhibitReason::RF_RUNTIME_ERROR);
}

bool WsprTransmitter::selectNextBand() {
  return false;
}

bool WsprTransmitter::setFrequencyMode(WsprFrequencyMode mode) {
  if (state_ == TransmitterState::PREPARING ||
      state_ == TransmitterState::TRANSMITTING) {
    return false;
  }
  config_.frequencyMode = mode;
  if (mode == WsprFrequencyMode::FIXED) {
    transmitFrequencyHz_ = static_cast<uint32_t>(
        static_cast<int64_t>(config_.baseFrequencyHz) +
        config_.frequencyOffsetHz);
  }
  return true;
}

void WsprTransmitter::setTxAdmissionAllowed(bool allowed) {
  if (txAdmissionAllowed_ == allowed) return;
  txAdmissionAllowed_ = allowed;
  if (!allowed && configurationSessionSafe()) scheduler_.disarm();
}

const char *WsprTransmitter::bandName() const {
  const BandPlanEntry *band = BandPlan::find(config_.band);
  return band == nullptr ? "--" : band->name;
}

WsprMessageSnapshot WsprTransmitter::messageSnapshot(
    const LocatorSnapshot &location) const {
  WsprMessageSnapshot result;
  strncpy(result.callsign, config_.callsign, sizeof(result.callsign));
  result.callsign[sizeof(result.callsign) - 1] = '\0';
  const char *locator = config_.useGpsLocator
      ? location.locator : config_.fixedLocator;
  strncpy(result.locator, locator, sizeof(result.locator));
  result.locator[sizeof(result.locator) - 1] = '\0';
  result.powerDbm = config_.powerDbm;
  return result;
}

bool WsprTransmitter::prepareTransmission(const LocatorSnapshot &location) {
  if (!rf_.operational()) {
    state_ = TransmitterState::RF_INHIBITED;
    telemetry_.setInhibitReason(TxInhibitReason::RF_UNAVAILABLE);
    return false;
  }
  prepared_ = TxPreparedSnapshot();
  firstSymbolPrepared_ = false;
  const uint32_t fixedFrequencyHz = static_cast<uint32_t>(
      static_cast<int64_t>(config_.baseFrequencyHz) +
      config_.frequencyOffsetHz);
  transmitFrequencyHz_ = frequencySelector_.select(
      config_.frequencyMode, config_.band, fixedFrequencyHz);

  prepared_.message = messageSnapshot(location);
  prepared_.frequencyHz = transmitFrequencyHz_;
  prepared_.band = config_.band;
  prepared_.frequencyMode = config_.frequencyMode;
  prepared_.authorization = scheduler_.authorization();
  const uint32_t cycleMillis =
      static_cast<uint32_t>(config_.transmitIntervalMinutes) * 60000UL;
  prepared_.slotUtcMillis = cycleMillis == 0
      ? 0 : (scheduler_.slot() * cycleMillis) % 86400000UL;
  encoder_.wspr_encode(prepared_.message.callsign, prepared_.message.locator,
                       prepared_.message.powerDbm,
                       symbols_);
  prepared_.valid = true;
  state_ = TransmitterState::PREPARING;
  return true;
}

bool WsprTransmitter::prepareFirstSymbol() {
  if (!prepared_.valid || !rf_.operational() ||
      !rf_.prepareFrequency(prepared_.frequencyHz * 100ULL +
                            symbols_[0] * TONE_SPACING_CENTIHZ)) {
    abortTransmission(TxInhibitReason::RF_UNAVAILABLE);
    return false;
  }
  const TxTimeSource timeSource =
      prepared_.authorization == WsprTimeAuthorization::GPS_PRECISE
          ? TxTimeSource::GPS : TxTimeSource::NTP;
  telemetry_.prepareFrame(prepared_.slotUtcMillis, timeSource,
                          prepared_.message.callsign,
                          prepared_.message.locator,
                          prepared_.message.powerDbm,
                          prepared_.frequencyHz);
  telemetry_.setInhibitReason(TxInhibitReason::NONE);
  firstSymbolPrepared_ = true;
  return true;
}

bool WsprTransmitter::releasePreparation() {
  if (state_ != TransmitterState::PREPARING) return false;
  if (firstSymbolPrepared_) return true;
  if (!prepareFirstSymbol()) {
    scheduler_.disarm();
    return false;
  }
  return true;
}

bool WsprTransmitter::preparationMarginMillis(
    uint32_t nowMillis, uint32_t &remainingMillis) const {
  if (state_ != TransmitterState::PREPARING || !prepared_.valid) return false;
  return scheduler_.millisUntilSlot(nowMillis, remainingMillis);
}

bool WsprTransmitter::startTransmission() {
  if (!txAdmissionAllowed_ || !firstSymbolPrepared_ || !rf_.enableOutput()) {
    abortTransmission(TxInhibitReason::RF_RUNTIME_ERROR);
    return false;
  }
  scheduler_.startPreparedSymbols();
  telemetry_.frameStarted();
  state_ = TransmitterState::TRANSMITTING;
  return true;
}

void WsprTransmitter::finishTransmission() {
  const bool completed = state_ == TransmitterState::TRANSMITTING;
  rf_.disableOutput();
  scheduler_.disarm();
  if (completed) telemetry_.frameCompleted();
  prepared_ = TxPreparedSnapshot();
  firstSymbolPrepared_ = false;
  state_ = TransmitterState::IDLE;
}

void WsprTransmitter::abortTransmission(TxInhibitReason reason) {
  const bool interrupted = state_ == TransmitterState::TRANSMITTING;
  rf_.disableOutput();
  scheduler_.disarm();
  if (interrupted) telemetry_.frameInterrupted(reason);
  telemetry_.setInhibitReason(reason);
  prepared_ = TxPreparedSnapshot();
  firstSymbolPrepared_ = false;
  state_ = TransmitterState::RF_INHIBITED;
}

void WsprTransmitter::update(const TimeSnapshot &time,
                             const LocatorSnapshot &location,
                             uint32_t nowMillis, uint32_t nowMicros) {
  if (!identityConfigured_) {
    rf_.disableOutput();
    scheduler_.disarm();
    prepared_ = TxPreparedSnapshot();
    firstSymbolPrepared_ = false;
    state_ = TransmitterState::RF_INHIBITED;
    telemetry_.setInhibitReason(
        TxInhibitReason::OPERATING_IDENTITY_UNCONFIGURED);
    return;
  }
  if (!rf_.operational()) {
    if (state_ == TransmitterState::PREPARING ||
        state_ == TransmitterState::TRANSMITTING) {
      abortTransmission(state_ == TransmitterState::TRANSMITTING
                            ? TxInhibitReason::RF_RUNTIME_ERROR
                            : TxInhibitReason::RF_UNAVAILABLE);
    } else {
      state_ = TransmitterState::RF_INHIBITED;
      telemetry_.setInhibitReason(TxInhibitReason::RF_UNAVAILABLE);
    }
    return;
  }
  if (state_ == TransmitterState::PREPARING) {
    // OLED acknowledgement is never required. Once symbol 0 is prepared with
    // CLK0 disabled, wait non-blockingly for the armed monotonic slot.
    if (!firstSymbolPrepared_ && !releasePreparation()) return;
    const WsprStartDecision decision = scheduler_.startDecision(nowMillis);
    if (decision == WsprStartDecision::START &&
        scheduler_.slot() != activeSlot_) {
      activeSlot_ = scheduler_.slot();
      startTransmission();
    } else if (decision == WsprStartDecision::MISSED) {
      telemetry_.setInhibitReason(TxInhibitReason::SLOT_MISSED);
      finishTransmission();
    }
    return;
  }
  if (state_ == TransmitterState::TRANSMITTING) {
    if (scheduler_.symbolDue(nowMicros)) {
      uint16_t symbolIndex = 0;
      if (!scheduler_.takeSymbol(symbolIndex)) {
        finishTransmission();
        return;
      }
      if (!rf_.prepareFrequency(
              transmitFrequencyHz_ * 100ULL +
              symbols_[symbolIndex] * TONE_SPACING_CENTIHZ)) {
        abortTransmission(TxInhibitReason::RF_RUNTIME_ERROR);
        return;
      }
    }
    return;
  }

  if (!txAdmissionAllowed_) {
    scheduler_.disarm();
    return;
  }

  if (!timeAuthorized(time)) {
    state_ = TransmitterState::WAITING_FOR_TIME;
    telemetry_.setInhibitReason(time.utcValid()
        ? TxInhibitReason::TIME_UNQUALIFIED
        : TxInhibitReason::TIME_UNAVAILABLE);
    return;
  }
  if (state_ == TransmitterState::WAITING_FOR_TIME) {
    state_ = TransmitterState::IDLE;
    telemetry_.setInhibitReason(TxInhibitReason::NONE);
  }

  if (state_ == TransmitterState::IDLE &&
      scheduler_.arm(time, nowMillis, nowMicros,
                     config_.transmitIntervalMinutes,
                     config_.preparationSeconds) &&
      scheduler_.slot() != activeSlot_) {
    if (!prepareTransmission(location)) scheduler_.disarm();
  }

}

const char *WsprTransmitter::stateLabel() const {
  switch (state_) {
    case TransmitterState::RF_INHIBITED: return "RF INHIBITED";
    case TransmitterState::WAITING_FOR_TIME: return "WAIT TIME";
    case TransmitterState::IDLE: return "TX OFF";
    case TransmitterState::PREPARING: return "PREPARING";
    case TransmitterState::TRANSMITTING: return "TX WSPR";
  }
  return "UNKNOWN";
}
