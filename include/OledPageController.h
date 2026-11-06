#pragma once

#include <stdint.h>

enum class OledOperationalPage : uint8_t { TIME_STATUS, IDLE_STATUS };

class OledPageController {
 public:
  static constexpr uint32_t PAGE_DURATION_MS = 5000UL;

  OledOperationalPage update(uint32_t nowMillis, bool timeQualified,
                             bool timingCritical);
  OledOperationalPage page() const { return page_; }

 private:
  OledOperationalPage page_ = OledOperationalPage::TIME_STATUS;
  uint32_t pageStartedMillis_ = 0;
  uint32_t pauseStartedMillis_ = 0;
  bool timeQualified_ = false;
  bool paused_ = false;
  bool initialized_ = false;
};
