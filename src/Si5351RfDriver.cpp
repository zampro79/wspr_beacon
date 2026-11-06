#include "Si5351RfDriver.h"

#include <Wire.h>

#include "BoardConfig.h"

bool Si5351RfDriver::detected() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
#else
  Wire.begin();
#endif
  Wire.beginTransmission(I2C_ADDRESS);
  return Wire.endTransmission() == 0;
}

bool Si5351RfDriver::initialize(uint32_t referenceHz, int32_t correction) {
  // The Si5351 may remain powered across an MCU reset. Gate every physical
  // output before the library reset sequence changes clock-control registers.
  disableAllOutputs();
  if (!synthesizer_.init(SI5351_CRYSTAL_LOAD_8PF, referenceHz, correction)) {
    return false;
  }
  disableAllOutputs();
  synthesizer_.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  synthesizer_.set_clock_pwr(SI5351_CLK0, 0);
  synthesizer_.output_enable(SI5351_CLK0, 0);
  outputEnabled_ = false;
  return true;
}

bool Si5351RfDriver::setFrequency(uint64_t centihertz) {
  if (!outputEnabled_) {
    synthesizer_.output_enable(SI5351_CLK0, 0);
    synthesizer_.set_clock_pwr(SI5351_CLK0, 0);
  }
  const bool configured = synthesizer_.set_freq(centihertz, SI5351_CLK0) == 0;
  // Etherkit set_freq() enables the global output gate on its first call.
  // Restore both gates whenever frequency is prepared with RF intentionally off.
  if (!outputEnabled_) {
    synthesizer_.output_enable(SI5351_CLK0, 0);
    synthesizer_.set_clock_pwr(SI5351_CLK0, 0);
  }
  return configured;
}

void Si5351RfDriver::enableOutput() {
  synthesizer_.set_clock_pwr(SI5351_CLK0, 1);
  synthesizer_.output_enable(SI5351_CLK0, 1);
  outputEnabled_ = true;
}

void Si5351RfDriver::disableOutput() {
  synthesizer_.output_enable(SI5351_CLK0, 0);
  synthesizer_.set_clock_pwr(SI5351_CLK0, 0);
  outputEnabled_ = false;
}

void Si5351RfDriver::disableAllOutputs() {
  for (uint8_t clock = static_cast<uint8_t>(SI5351_CLK0);
       clock <= static_cast<uint8_t>(SI5351_CLK7); ++clock) {
    const si5351_clock output = static_cast<si5351_clock>(clock);
    synthesizer_.output_enable(output, 0);
    synthesizer_.set_clock_pwr(output, 0);
  }
  outputEnabled_ = false;
}
