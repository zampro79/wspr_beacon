#pragma once

#include <stdint.h>

class RfDriver {
 public:
  virtual ~RfDriver() = default;
  virtual bool detected() = 0;
  virtual bool initialize(uint32_t referenceHz, int32_t correction) = 0;
  virtual bool setFrequency(uint64_t centihertz) = 0;
  virtual void enableOutput() = 0;
  virtual void disableOutput() = 0;
};
