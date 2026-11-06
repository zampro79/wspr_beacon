#pragma once

#include "ConfigurationManager.h"
#include "FactoryResetCore.h"

enum class BootConfigurationDomain : uint8_t { WSPR, WIFI };
enum class BootConfigurationWarning : uint8_t {
  NONE,
  WSPR_SLOT_REJECTED_RECOVERED_SAVED,
  WIFI_SLOT_REJECTED_RECOVERED_SAVED,
  WSPR_NO_VALID_RECORD_USING_DEFAULTS,
  WIFI_NO_VALID_RECORD_USING_DEFAULTS,
};

namespace BootConfigurationDiagnostics {
inline BootConfigurationWarning classify(
    BootConfigurationDomain domain, bool recordRejected, ConfigOrigin origin,
    FactoryResetBootState factoryResetState) {
  if (!recordRejected ||
      factoryResetState == FactoryResetBootState::FAIL_CLOSED) {
    return BootConfigurationWarning::NONE;
  }
  if (origin == ConfigOrigin::SAVED) {
    return domain == BootConfigurationDomain::WSPR
               ? BootConfigurationWarning::WSPR_SLOT_REJECTED_RECOVERED_SAVED
               : BootConfigurationWarning::WIFI_SLOT_REJECTED_RECOVERED_SAVED;
  }
  return domain == BootConfigurationDomain::WSPR
             ? BootConfigurationWarning::WSPR_NO_VALID_RECORD_USING_DEFAULTS
             : BootConfigurationWarning::WIFI_NO_VALID_RECORD_USING_DEFAULTS;
}
}  // namespace BootConfigurationDiagnostics
