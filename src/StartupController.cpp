#include "StartupController.h"

void StartupController::begin(uint32_t monotonicMillis) {
  startedAtMillis_ = monotonicMillis;
  started_ = true;
}

bool StartupController::active(uint32_t monotonicMillis) const {
  return started_ &&
         static_cast<uint32_t>(monotonicMillis - startedAtMillis_) <
             DURATION_MS;
}
