#pragma once

#include <stdint.h>

enum class TxDisplayAction : uint8_t {
  NONE,
  RENDER_PREPARING,
  RENDER_TX_ON,
  SKIP
};

class TxDisplayCoordinator {
 public:
  // A full SSD1306 frame is about 25 ms at 400 kHz and under 100 ms at
  // 100 kHz. 500 ms leaves at least a 5x reserve for bus and RF setup.
  static constexpr uint32_t MINIMUM_MARGIN_MS = 500UL;
  static constexpr uint32_t TX_ON_LEAD_MS = 1000UL;

  TxDisplayAction update(bool preparationReady, bool displayAvailable,
                         bool marginValid, uint32_t remainingMillis);
  void reset();

 private:
  bool preparingAttempted_ = false;
  bool txOnAttempted_ = false;
};
