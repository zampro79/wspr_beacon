#pragma once

#include "BeaconConfig.h"
#include "BeaconSettings.h"

class WsprConfigAdapter {
 public:
  static bool apply(const BeaconSettings &settings, bool transmitterBusy,
                    BeaconConfig &target);
};
