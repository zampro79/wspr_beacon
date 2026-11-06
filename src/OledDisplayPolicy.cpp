#include "OledDisplayPolicy.h"

OledDisplayTarget OledDisplayPolicy::select(SystemMode systemMode,
                                             bool hardwareCheckActive) {
  if (systemMode == SystemMode::PROVISIONING_AP) {
    return OledDisplayTarget::PROVISIONING;
  }
  if (systemMode == SystemMode::PROVISIONING_ERROR) {
    return OledDisplayTarget::PROVISIONING_ERROR;
  }
  return hardwareCheckActive ? OledDisplayTarget::HARDWARE_CHECK
                             : OledDisplayTarget::OPERATIONAL;
}
