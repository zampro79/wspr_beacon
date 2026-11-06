#pragma once

#include "ApSetupMarker.h"
#include "FactoryResetCore.h"

namespace ApSetupBootDecision {
inline bool trySta(bool storageReady, bool savedCredentialsUsable,
                   FactoryResetBootState factoryState,
                   ApSetupMarkerState markerState) {
  return storageReady && savedCredentialsUsable &&
         factoryState != FactoryResetBootState::FAIL_CLOSED &&
         (factoryState != FactoryResetBootState::NORMAL ||
          markerState == ApSetupMarkerState::ABSENT ||
          markerState == ApSetupMarkerState::CONSUMED);
}
}  // namespace ApSetupBootDecision
