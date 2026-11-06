#pragma once

#include <stddef.h>
#include <stdint.h>

enum class DiagnosticModule : uint8_t {
  GPS,
  PPS,
  SI5351A,
  OLED,
  WIFI,
  NTP
};

enum class DiagnosticState : uint8_t {
  STATE_UNKNOWN,
  STATE_DISABLED,
  STATE_NOT_DETECTED,
  STATE_INITIALIZING,
  STATE_OPERATIONAL,
  STATE_DEGRADED,
  STATE_ERROR
};

enum class DiagnosticReason : uint8_t {
  NONE,
  NOT_EVALUATED,
  DISABLED_BY_CONFIGURATION,
  DEVICE_NOT_FOUND,
  INITIALIZATION_FAILED,
  COMMUNICATION_FAILURE,
  TIMEOUT,
  INVALID_DATA,
  SERVICE_UNAVAILABLE,
  WAITING_FOR_DATA,
  NOT_IMPLEMENTED,
  RF_OUTPUT_UNVERIFIED,
  INTERNAL_ERROR
};

constexpr size_t DIAGNOSTIC_MODULE_COUNT = 6;

struct DiagnosticCounters {
  uint16_t stateChanges = 0;
  uint16_t successes = 0;
  uint16_t failures = 0;
};

struct DiagnosticRecord {
  uint32_t lastChangeMillis = 0;
  DiagnosticCounters counters;
  DiagnosticState state = DiagnosticState::STATE_UNKNOWN;
  DiagnosticReason reason = DiagnosticReason::NOT_EVALUATED;
};

struct DiagnosticSnapshot {
  DiagnosticRecord modules[DIAGNOSTIC_MODULE_COUNT];
};

class DiagnosticRegistry {
 public:
  DiagnosticRegistry();

  bool setState(DiagnosticModule module, DiagnosticState state,
                DiagnosticReason reason, uint32_t timestampMillis);
  bool recordSuccess(DiagnosticModule module, uint16_t increment = 1);
  bool recordFailure(DiagnosticModule module, uint16_t increment = 1);
  bool read(DiagnosticModule module, DiagnosticRecord &record) const;
  void snapshot(DiagnosticSnapshot &snapshot) const;

 private:
  DiagnosticRecord modules_[DIAGNOSTIC_MODULE_COUNT];

  static bool moduleIndex(DiagnosticModule module, size_t &index);
  static void saturatingAdd(uint16_t &counter, uint16_t increment);
};

extern DiagnosticRegistry diagnostics;

static_assert(sizeof(DiagnosticRecord) == 12, "DiagnosticRecord must remain compact");
static_assert(sizeof(DiagnosticSnapshot) == 72, "DiagnosticSnapshot must remain compact");
