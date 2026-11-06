#pragma once

#include <stdint.h>

#include "RfDriver.h"

enum class RfFault : uint8_t {
  NONE,
  NOT_DETECTED,
  INITIALIZATION_FAILED,
  FREQUENCY_CONFIGURATION_FAILED,
  CRITICAL_RUNTIME_ERROR
};

class RfSafetyController {
 public:
  RfSafetyController(RfDriver &driver, uint32_t referenceHz,
                     int32_t correction)
      : driver_(driver), referenceHz_(referenceHz), correction_(correction) {}

  bool begin();
  bool recover();
  bool configure(uint32_t referenceHz, int32_t correction);
  bool prepareFrequency(uint64_t centihertz);
  bool enableOutput();
  void disableOutput();
  void reportCriticalError();

  bool operational() const { return fault_ == RfFault::NONE; }
  bool outputEnabled() const { return outputEnabled_; }
  RfFault fault() const { return fault_; }
  int32_t correction() const { return correction_; }

 private:
  RfDriver &driver_;
  uint32_t referenceHz_;
  int32_t correction_;
  RfFault fault_ = RfFault::INITIALIZATION_FAILED;
  bool frequencyPrepared_ = false;
  bool outputEnabled_ = false;

  void fail(RfFault fault);
};
