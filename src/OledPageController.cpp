#include "OledPageController.h"

OledOperationalPage OledPageController::update(uint32_t nowMillis,
                                                bool timeQualified,
                                                bool timingCritical) {
  if (!initialized_) {
    initialized_ = true;
    pageStartedMillis_ = nowMillis;
    timeQualified_ = timeQualified;
  }

  if (timingCritical) {
    if (!paused_) {
      paused_ = true;
      pauseStartedMillis_ = nowMillis;
    }
    return page_;
  }

  if (paused_) {
    pageStartedMillis_ +=
        static_cast<uint32_t>(nowMillis - pauseStartedMillis_);
    paused_ = false;
  }

  if (!timeQualified) {
    page_ = OledOperationalPage::TIME_STATUS;
    pageStartedMillis_ = nowMillis;
    timeQualified_ = false;
    return page_;
  }

  if (!timeQualified_) {
    page_ = OledOperationalPage::TIME_STATUS;
    pageStartedMillis_ = nowMillis;
    timeQualified_ = true;
    return page_;
  }

  if (static_cast<uint32_t>(nowMillis - pageStartedMillis_) >=
      PAGE_DURATION_MS) {
    page_ = page_ == OledOperationalPage::TIME_STATUS
        ? OledOperationalPage::IDLE_STATUS
        : OledOperationalPage::TIME_STATUS;
    pageStartedMillis_ = nowMillis;
  }
  return page_;
}
