#include "DiagnosticRegistry.h"

#include <string.h>

DiagnosticRegistry diagnostics;

DiagnosticRegistry::DiagnosticRegistry() {
  for (size_t index = 0; index < DIAGNOSTIC_MODULE_COUNT; ++index) {
    modules_[index] = DiagnosticRecord{};
  }
}

bool DiagnosticRegistry::moduleIndex(DiagnosticModule module, size_t &index) {
  index = static_cast<size_t>(module);
  return index < DIAGNOSTIC_MODULE_COUNT;
}

void DiagnosticRegistry::saturatingAdd(uint16_t &counter, uint16_t increment) {
  const uint32_t sum = static_cast<uint32_t>(counter) + increment;
  counter = sum > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(sum);
}

bool DiagnosticRegistry::setState(DiagnosticModule module, DiagnosticState state,
                                  DiagnosticReason reason, uint32_t timestampMillis) {
  size_t index = 0;
  if (!moduleIndex(module, index)) return false;
  DiagnosticRecord &record = modules_[index];
  if (record.state == state && record.reason == reason) return false;
  record.state = state;
  record.reason = reason;
  record.lastChangeMillis = timestampMillis;
  saturatingAdd(record.counters.stateChanges, 1);
  return true;
}

bool DiagnosticRegistry::recordSuccess(DiagnosticModule module, uint16_t increment) {
  size_t index = 0;
  if (!moduleIndex(module, index)) return false;
  saturatingAdd(modules_[index].counters.successes, increment);
  return true;
}

bool DiagnosticRegistry::recordFailure(DiagnosticModule module, uint16_t increment) {
  size_t index = 0;
  if (!moduleIndex(module, index)) return false;
  saturatingAdd(modules_[index].counters.failures, increment);
  return true;
}

bool DiagnosticRegistry::read(DiagnosticModule module, DiagnosticRecord &record) const {
  size_t index = 0;
  if (!moduleIndex(module, index)) return false;
  record = modules_[index];
  return true;
}

void DiagnosticRegistry::snapshot(DiagnosticSnapshot &snapshot) const {
  memcpy(snapshot.modules, modules_, sizeof(modules_));
}
