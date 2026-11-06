#include "ConsoleStatus.h"

ConsoleStatusSnapshot ConsoleStatus::build(const ConsoleStatusInputs &inputs) {
  ConsoleStatusSnapshot result;
  result.uptimeMillis = inputs.nowMillis;
  result.configuredFrequencyHz = inputs.configuredFrequencyHz;
  result.frequencyMode = inputs.frequencyMode;
  result.time = inputs.time;
  result.location = inputs.location;
  result.diagnostics = inputs.diagnostics;
  result.authorization = inputs.authorization;
  result.wifiState = inputs.wifiState;
  result.txState = inputs.txState;
  result.txTelemetry = inputs.txTelemetry;
  result.http = inputs.http;
  result.operatingMode = inputs.operatingMode;

  if (WsprScheduler::nextSlotUtcMillis(
          inputs.time, inputs.transmitIntervalMinutes,
          result.nextSlotUtcMillis)) {
    result.flags |= 1U << 0;
  }
  if (inputs.monitorEnabled) result.flags |= 1U << 1;
  if (inputs.candidateDirty) result.flags |= 1U << 2;
  if (inputs.wifiSaved) result.flags |= 1U << 3;
  if (inputs.gpsUtcValid) result.flags |= 1U << 4;
  if (inputs.gpsPositionValid) result.flags |= 1U << 5;
  if (inputs.ppsPresent) result.flags |= 1U << 6;
  if (inputs.ppsAssociated) result.flags |= 1U << 7;

  return result;
}
