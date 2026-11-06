#include "DisplayController.h"

#include <string.h>

void DisplayController::beginPowerSave(bool enabled) {
  powerSave_.begin(enabled, millis());
}

void DisplayController::setPowerSave(bool enabled) {
  powerSave_.setEnabled(enabled, millis());
}

void DisplayController::servicePowerSave(uint32_t nowMillis,
                                         bool timingCritical) {
  const PowerSaveDisplayAction action =
      powerSave_.update(nowMillis, timingCritical, initialized_);
  if (!initialized_) return;
  if (action == PowerSaveDisplayAction::DISPLAY_OFF) {
    display_.ssd1306_command(SSD1306_DISPLAYOFF);
  } else if (action == PowerSaveDisplayAction::DISPLAY_ON) {
    display_.ssd1306_command(SSD1306_DISPLAYON);
    invalidateDrawingCaches();
  }
}

void DisplayController::notePresentation(PowerSavePresentation presentation,
                                         uint32_t nowMillis) {
  powerSave_.setPresentation(presentation, nowMillis);
}

void DisplayController::invalidateDrawingCaches() {
  hardwareCheckRevision_ = 0;
  operationalDrawn_ = configurationDrawn_ = provisioningDrawn_ = false;
  provisioningErrorDrawn_ = apOperationalDrawn_ = false;
}

bool DisplayController::begin() {
  if (!display_.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false)) return false;
  initialized_ = true;
  display_.setRotation(2);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.clearDisplay();

  // Antenna, onde radio e traliccio reticolare nell'area sinistra.
  display_.drawLine(17, 2, 17, 12, SSD1306_WHITE);
  display_.drawLine(21, 4, 25, 2, SSD1306_WHITE);
  display_.drawLine(23, 7, 32, 7, SSD1306_WHITE);
  display_.drawLine(21, 10, 25, 12, SSD1306_WHITE);
  display_.drawTriangle(17, 12, 4, 58, 30, 58, SSD1306_WHITE);
  display_.drawLine(13, 24, 21, 24, SSD1306_WHITE);
  display_.drawLine(10, 36, 24, 36, SSD1306_WHITE);
  display_.drawLine(7, 48, 27, 48, SSD1306_WHITE);
  display_.drawLine(13, 24, 24, 36, SSD1306_WHITE);
  display_.drawLine(21, 24, 10, 36, SSD1306_WHITE);
  display_.drawLine(10, 36, 27, 48, SSD1306_WHITE);
  display_.drawLine(24, 36, 7, 48, SSD1306_WHITE);
  display_.drawLine(7, 48, 30, 58, SSD1306_WHITE);
  display_.drawLine(27, 48, 4, 58, SSD1306_WHITE);
  display_.drawLine(2, 58, 32, 58, SSD1306_WHITE);

  const StartupBannerBounds separator = StartupBanner::separatorBounds();
  display_.drawFastHLine(separator.x, separator.y, separator.width,
                         SSD1306_WHITE);
  for (uint8_t index = 0; index < StartupBanner::LINE_COUNT; ++index) {
    const StartupBannerLine line = StartupBanner::line(index);
    display_.setCursor(line.x, line.y);
    display_.print(line.text);
  }
  display_.display();
  return true;
}

namespace {
const char *checkLabel(HardwareCheckResult result) {
  switch (result) {
    case HardwareCheckResult::WAIT: return "WAIT";
    case HardwareCheckResult::OK: return "OK";
    case HardwareCheckResult::FAIL: return "FAIL";
  }
  return "FAIL";
}
}  // namespace

void DisplayController::showHardwareCheck(
    const HardwareCheckSnapshot &snapshot) {
  if (!renderAllowed() || snapshot.revision == hardwareCheckRevision_) return;
  hardwareCheckRevision_ = snapshot.revision;
  display_.setTextWrap(false);
  display_.clearDisplay();
  display_.setCursor(22, 0);
  display_.print(F("HARDWARE CHECK"));
  const char *labels[] = {"OLED", "GPS", "Si5351A", "LED"};
  const HardwareCheckResult results[] = {
      snapshot.oled, snapshot.gps, snapshot.si5351, snapshot.led};
  const uint8_t y[] = {16, 28, 40, 52};
  for (uint8_t index = 0; index < 4; ++index) {
    display_.setCursor(4, y[index]);
    display_.print(labels[index]);
    display_.setCursor(92, y[index]);
    display_.print(checkLabel(results[index]));
  }
  display_.display();
  notePresentation(PowerSavePresentation::NONE, millis());
}

bool DisplayController::showTxPrepared(const TxPreparedSnapshot &prepared) {
  if (!renderAllowed() || !prepared.valid) return false;
  const TxStatusLayout layout = TxStatusScreen::build(prepared);
  pageController_.update(millis(), true, true);
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setCursor(TxStatusLayout::TITLE_X, TxStatusLayout::TITLE_Y);
  display_.print(F("WSPR TRANSMITTING"));
  display_.setTextSize(2);
  display_.setCursor(TxStatusLayout::TX_X, TxStatusLayout::TX_Y);
  display_.print(F("TX ON"));
  display_.setTextSize(1);
  display_.setCursor(layout.messageX, TxStatusLayout::MESSAGE_Y);
  display_.print(layout.message);
  display_.setCursor(layout.frequencyX, TxStatusLayout::FREQUENCY_Y);
  display_.print(layout.frequency);
  display_.display();
  notePresentation(PowerSavePresentation::NONE, millis());
  operationalDrawn_ = false;
  return true;
}

bool DisplayController::showPreparing(const TxPreparedSnapshot &prepared) {
  if (!renderAllowed() || !prepared.valid) return false;
  const PreparingStatusLayout layout = PreparingStatusScreen::build(prepared);
  pageController_.update(millis(), true, true);
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setCursor(PreparingStatusLayout::TITLE_X,
                     PreparingStatusLayout::TITLE_Y);
  display_.print(F("WSPR BEACON"));
  display_.setTextSize(2);
  display_.setCursor(PreparingStatusLayout::STATUS_X,
                     PreparingStatusLayout::STATUS_Y);
  display_.print(F("PREPARING"));
  display_.setTextSize(1);
  display_.setCursor(layout.messageX, PreparingStatusLayout::MESSAGE_Y);
  display_.print(layout.message);
  display_.setCursor(layout.frequencyX, PreparingStatusLayout::FREQUENCY_Y);
  display_.print(layout.frequency);
  display_.display();
  notePresentation(PowerSavePresentation::NONE, millis());
  operationalDrawn_ = false;
  return true;
}

void DisplayController::showProvisioning(const char *ssid,
                                         const char *password, bool error) {
  if (!renderAllowed()) return;
  if (provisioningDrawn_ && provisioningErrorDrawn_ == error) return;
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setCursor(0, 0); display_.print(F("PROVISIONING"));
  display_.setCursor(0, 12); display_.print(F("TX DISABLED"));
  if (error) {
    display_.setCursor(0, 30); display_.print(F("AP START ERROR"));
  } else {
    display_.setCursor(0, 24); display_.print(ssid == nullptr ? "" : ssid);
    display_.setCursor(0, 36); display_.print(F("PASS: "));
    display_.print(password == nullptr ? "" : password);
    display_.setCursor(0, 52); display_.print(F("192.168.4.1"));
  }
  display_.display();
  notePresentation(error ? PowerSavePresentation::AP_START_ERROR
                         : PowerSavePresentation::AP_PROVISIONING,
                   millis());
  provisioningDrawn_ = true;
  provisioningErrorDrawn_ = error;
  apOperationalDrawn_ = false;
  operationalDrawn_ = false;
}

void DisplayController::showApOperational(const char *ssid,
                                          const char *password) {
  if (!renderAllowed() || apOperationalDrawn_) return;
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setCursor(0, 0); display_.print(F("AP OPERATIONAL"));
  display_.setCursor(0, 12); display_.print(F("TX DISABLED"));
  display_.setCursor(0, 24); display_.print(ssid == nullptr ? "" : ssid);
  display_.setCursor(0, 36); display_.print(F("PASS: "));
  display_.print(password == nullptr ? "" : password);
  display_.setCursor(0, 52); display_.print(F("192.168.4.1"));
  display_.display();
  notePresentation(PowerSavePresentation::AP_OPERATIONAL, millis());
  provisioningDrawn_ = false;
  apOperationalDrawn_ = true;
  operationalDrawn_ = false;
}

void DisplayController::update(const OperationalStatusInputs &inputs,
                               const IdleStatusInputs &idleInputs,
                               const WsprTransmitter &transmitter,
                               bool startupActive,
                               const ConfigurationSessionSnapshot &configurationSession) {
  if (!renderAllowed()) return;
  const uint32_t now = millis();
  const bool rfTimingCritical =
      transmitter.state() == TransmitterState::PREPARING ||
      transmitter.state() == TransmitterState::TRANSMITTING;
  const OledOperationalPage page = pageController_.update(
      now, inputs.authorization != WsprTimeAuthorization::NONE,
      rfTimingCritical);
  if (startupActive || rfTimingCritical) return;
  if (ConfigurationStatusScreen::visible(configurationSession,
                                         idleInputs.rfState)) {
    const ConfigurationStatusSnapshot snapshot =
        ConfigurationStatusScreen::build(configurationSession);
    if (configurationDrawn_ &&
        memcmp(&snapshot, &configurationSnapshot_, sizeof(snapshot)) == 0) {
      return;
    }
    configurationSnapshot_ = snapshot;
    display_.clearDisplay();
    display_.setTextWrap(false);
    const uint8_t y[] = {8, 28, 48};
    for (uint8_t index = 0; index < ConfigurationStatusSnapshot::LINE_COUNT;
         ++index) {
      const uint8_t width = strlen(snapshot.lines[index]) * 6U;
      display_.setCursor(width < 128U ? (128U - width) / 2U : 0U, y[index]);
      display_.print(snapshot.lines[index]);
    }
    display_.display();
    notePresentation(PowerSavePresentation::NONE, now);
    configurationDrawn_ = true;
    operationalDrawn_ = false;
    return;
  }
  configurationDrawn_ = false;
  if (operationalDrawn_ && page == renderedPage_ &&
      now - lastUpdateMillis_ < 100) return;
  lastUpdateMillis_ = now;

  const uint8_t y[] = {0, 12, 24, 36, 48};
  if (page == OledOperationalPage::TIME_STATUS) {
    const OperationalStatusSnapshot snapshot = operationalScreen_.update(inputs);
    if (operationalDrawn_ && renderedPage_ == page &&
        memcmp(&snapshot, &operationalSnapshot_, sizeof(snapshot)) == 0) {
      return;
    }
    operationalSnapshot_ = snapshot;
    display_.clearDisplay();
    display_.setTextWrap(false);
    for (uint8_t index = 0;
         index < OperationalStatusSnapshot::LINE_COUNT; ++index) {
      display_.setCursor(0, y[index]);
      display_.print(snapshot.lines[index]);
    }
  } else {
    const IdleStatusSnapshot snapshot = IdleStatusScreen::build(idleInputs);
    if (operationalDrawn_ && renderedPage_ == page &&
        memcmp(&snapshot, &idleSnapshot_, sizeof(snapshot)) == 0) {
      return;
    }
    idleSnapshot_ = snapshot;
    display_.clearDisplay();
    display_.setTextWrap(false);
    for (uint8_t index = 0; index < IdleStatusSnapshot::LINE_COUNT; ++index) {
      display_.setCursor(0, y[index]);
      display_.print(snapshot.lines[index]);
    }
  }
  operationalDrawn_ = true;
  renderedPage_ = page;
  display_.display();
  notePresentation(page == OledOperationalPage::TIME_STATUS &&
                           operationalScreen_.ipv4Visible()
                       ? PowerSavePresentation::STA_IPV4
                       : PowerSavePresentation::NONE,
                   now);
}
