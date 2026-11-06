#pragma once

#include <si5351.h>

#include "RfDriver.h"

class Si5351RfDriver : public RfDriver {
 public:
  explicit Si5351RfDriver(Si5351 &synthesizer)
      : synthesizer_(synthesizer) {}

  bool detected() override;
  bool initialize(uint32_t referenceHz, int32_t correction) override;
  bool setFrequency(uint64_t centihertz) override;
  void enableOutput() override;
  void disableOutput() override;

 private:
  static constexpr uint8_t I2C_ADDRESS = 0x60;
  Si5351 &synthesizer_;
  bool outputEnabled_ = false;

  void disableAllOutputs();
};
