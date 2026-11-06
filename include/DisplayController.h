#pragma once

#include <Adafruit_SSD1306.h>

#include "HardwareCheckController.h"
#include "ConfigurationStatusScreen.h"
#include "IdleStatusScreen.h"
#include "OledPageController.h"
#include "OperationalStatusScreen.h"
#include "PreparingStatusScreen.h"
#include "PowerSaveDisplayPolicy.h"
#include "StartupBanner.h"
#include "TxStatusScreen.h"
#include "WsprTransmitter.h"

class DisplayController {
 public:
  explicit DisplayController(Adafruit_SSD1306 &display) : display_(display) {}
  bool begin();
  bool available() const { return initialized_; }
  void beginPowerSave(bool enabled);
  void setPowerSave(bool enabled);
  void servicePowerSave(uint32_t nowMillis, bool timingCritical);
  bool powerSaveEnabled() const { return powerSave_.enabled(); }
  void showHardwareCheck(const HardwareCheckSnapshot &snapshot);
  bool showPreparing(const TxPreparedSnapshot &prepared);
  bool showTxPrepared(const TxPreparedSnapshot &prepared);
  void showProvisioning(const char *ssid, const char *password,
                        bool error = false);
  void showApOperational(const char *ssid, const char *password);
  void update(const OperationalStatusInputs &inputs,
              const IdleStatusInputs &idleInputs,
              const WsprTransmitter &transmitter, bool startupActive,
              const ConfigurationSessionSnapshot &configurationSession);

 private:
  Adafruit_SSD1306 &display_;
  uint32_t lastUpdateMillis_ = 0;
  bool initialized_ = false;
  uint8_t hardwareCheckRevision_ = 0;
  bool operationalDrawn_ = false;
  bool configurationDrawn_ = false;
  bool provisioningDrawn_ = false;
  bool provisioningErrorDrawn_ = false;
  bool apOperationalDrawn_ = false;
  OledOperationalPage renderedPage_ = OledOperationalPage::TIME_STATUS;
  OledPageController pageController_;
  OperationalStatusScreen operationalScreen_;
  OperationalStatusSnapshot operationalSnapshot_;
  IdleStatusSnapshot idleSnapshot_;
  ConfigurationStatusSnapshot configurationSnapshot_;
  PowerSaveDisplayPolicy powerSave_;

  bool renderAllowed() const { return initialized_ && powerSave_.renderAllowed(); }
  void notePresentation(PowerSavePresentation presentation,
                        uint32_t nowMillis);
  void invalidateDrawingCaches();
};
