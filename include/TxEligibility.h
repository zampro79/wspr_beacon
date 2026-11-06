#pragma once

struct TxEligibilityInputs {
  bool manualPermission = false;
  bool operationalMode = false;
  bool identityConfigured = false;
  bool rfReady = false;
  bool configurationActive = false;
  bool resumeGuardActive = false;
  bool timeQualified = false;
};

class TxEligibility {
 public:
  static bool evaluate(const TxEligibilityInputs &inputs) {
    return inputs.manualPermission && inputs.operationalMode &&
           inputs.identityConfigured && inputs.rfReady &&
           !inputs.configurationActive && !inputs.resumeGuardActive &&
           inputs.timeQualified;
  }
};
