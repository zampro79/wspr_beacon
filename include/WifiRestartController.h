#pragma once

#include <stdint.h>

class WifiRestartController {
 public:
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 5000UL;
  bool schedule(uint32_t nowMillis) {
    if (pending_) return false;
    pending_ = true;
    startedMillis_ = nowMillis;
    responseDelivered_ = false;
    return true;
  }
  void responseDelivered() { if (pending_) responseDelivered_ = true; }
  bool pending() const { return pending_; }
  bool ready(uint32_t nowMillis, bool clientActive, bool rfSafe) const {
    return pending_ && rfSafe &&
           ((responseDelivered_ && !clientActive) ||
            static_cast<uint32_t>(nowMillis - startedMillis_) >=
                RESPONSE_TIMEOUT_MS);
  }
 private:
  bool pending_ = false;
  bool responseDelivered_ = false;
  uint32_t startedMillis_ = 0;
};
