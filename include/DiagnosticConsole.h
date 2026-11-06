#pragma once

#include <Arduino.h>

#include "BeaconConfig.h"
#include "ConfigurationManager.h"
#include "ConfigurationSessionController.h"
#include "ConsoleStatus.h"
#include "DiagnosticRegistry.h"
#include "GpsReceiver.h"
#include "LocatorService.h"
#include "TimeService.h"
#include "WsprTransmitter.h"
#include "WifiManager.h"

enum class ConsoleStatusView : uint8_t {
  SUMMARY,
  GPS,
  TIME,
  RF,
  WIFI,
  MODULES,
  TX_STATUS,
  TX_LAST
};

class DiagnosticConsole {
 public:
  explicit DiagnosticConsole(Stream &output) : output_(output) {}
  void setHttpDiagnostics(const HttpDiagnosticsSnapshot *snapshot) {
    httpDiagnostics_ = snapshot;
  }
  void setConfigurationSessions(
      const ConfigurationSessionController *sessions) {
    configurationSessions_ = sessions;
  }
  void update(bool periodicEnabled, uint32_t nowMillis,
              const BeaconConfig &config,
              const GpsReceiver &gps, const TimeService &timeService,
              const LocatorSnapshot &locator,
              const WsprTransmitter &transmitter, const WifiManager &wifi,
              const ConfigurationManager *configuration,
              bool monitorEnabled, bool consoleCandidateDirty);
  void printNow(uint32_t nowMillis, const BeaconConfig &config,
                const GpsReceiver &gps, const TimeService &timeService,
                const LocatorSnapshot &locator,
                const WsprTransmitter &transmitter, const WifiManager &wifi,
                const ConfigurationManager *configuration,
                bool monitorEnabled, bool consoleCandidateDirty,
                ConsoleStatusView view = ConsoleStatusView::SUMMARY);

 private:
  Stream &output_;
  uint32_t lastSummaryMillis_ = 0;
  const HttpDiagnosticsSnapshot *httpDiagnostics_ = nullptr;
  const ConfigurationSessionController *configurationSessions_ = nullptr;

  ConsoleStatusSnapshot capture(uint32_t nowMillis, const BeaconConfig &config,
                                const GpsReceiver &gps,
                                const TimeService &timeService,
                                const LocatorSnapshot &locator,
                                const WsprTransmitter &transmitter,
                                const WifiManager &wifi,
                                const ConfigurationManager *configuration,
                                bool monitorEnabled,
                                bool consoleCandidateDirty) const;
  void printSummary(const ConsoleStatusSnapshot &snapshot);
  void printGps(const ConsoleStatusSnapshot &snapshot);
  void printTime(const ConsoleStatusSnapshot &snapshot);
  void printLocation(const ConsoleStatusSnapshot &snapshot);
  void printRadio(const ConsoleStatusSnapshot &snapshot);
  void printNetwork(const ConsoleStatusSnapshot &snapshot);
  void printModules(const ConsoleStatusSnapshot &snapshot);
  void printTxStatus(const ConsoleStatusSnapshot &snapshot);
  void printTxLast(const ConsoleStatusSnapshot &snapshot);
  void printModule(DiagnosticModule module, const DiagnosticRecord &record,
                   uint32_t nowMillis);
  void printState(DiagnosticState state);
  void printReason(DiagnosticReason reason);
  void printUtc(uint32_t millisOfDay);
  void printTimeSource(TimeSource source);
  void printTimeQuality(TimeQuality quality);
  void printAuthorization(WsprTimeAuthorization authorization);
  void printTxInhibitReason(TxInhibitReason reason);
  void printTxTimeSource(TxTimeSource source);
  void printTxState(ConsoleTxState state);
  void printWifiState(WifiConnectionState state);
  static void writeSummaryLine(void *context, const char *line);
};
