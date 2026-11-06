#pragma once

#include <Arduino.h>

#if defined(ARDUINO_AVR_NANO_EVERY)
  #define WSPR_ISR_ATTR
  #define WSPR_BOARD_NAME "Nano Every"
  #define WSPR_HAS_WIFI 0
  #define WSPR_GPS_SERIAL1 1
  constexpr uint8_t PIN_BUTTON = 2;
  constexpr uint8_t PIN_GPS_PPS = 3;
  constexpr uint8_t PIN_STATUS_LED = LED_BUILTIN;
  constexpr bool STATUS_LED_ACTIVE_LOW = false;
#elif defined(ARDUINO_ARCH_ESP8266)
  #define WSPR_ISR_ATTR IRAM_ATTR
  #define WSPR_BOARD_NAME "ESP8266"
  #define WSPR_HAS_WIFI 1
  #define WSPR_GPS_SOFTWARE_SERIAL 1
  // D3/GPIO0 is a boot strap pin: do not hold the button during reset.
  constexpr uint8_t PIN_BUTTON = D3;
  constexpr uint8_t PIN_GPS_PPS = D5;
  constexpr uint8_t PIN_STATUS_LED = LED_BUILTIN;
  constexpr bool STATUS_LED_ACTIVE_LOW = true;
  constexpr uint8_t PIN_TIME_AUTHORITY_LED = D0;
  constexpr bool TIME_AUTHORITY_LED_ACTIVE_LOW = true;
  constexpr uint8_t PIN_GPS_RX = D6;
  constexpr uint8_t PIN_GPS_TX = D7;
#elif defined(ARDUINO_ARCH_ESP32)
  #define WSPR_ISR_ATTR IRAM_ATTR
  #define WSPR_BOARD_NAME "ESP32"
  #define WSPR_HAS_WIFI 1
  #define WSPR_GPS_SERIAL2 1
  constexpr uint8_t PIN_BUTTON = 25;
  constexpr uint8_t PIN_GPS_PPS = 26;
  constexpr uint8_t PIN_STATUS_LED = 2;
  constexpr bool STATUS_LED_ACTIVE_LOW = false;
  constexpr uint8_t PIN_GPS_RX = 16;
  constexpr uint8_t PIN_GPS_TX = 17;
  constexpr uint8_t PIN_I2C_SDA = 21;
  constexpr uint8_t PIN_I2C_SCL = 22;
#else
  #error "Unsupported board: use nano_every, esp8266 or esp32"
#endif

inline void setStatusLed(bool enabled) {
  digitalWrite(PIN_STATUS_LED, enabled != STATUS_LED_ACTIVE_LOW ? HIGH : LOW);
}

#if defined(ARDUINO_ARCH_ESP8266)
#include "StatusLedPolarity.h"

inline void setTimeAuthorityLed(bool enabled) {
  digitalWrite(PIN_TIME_AUTHORITY_LED,
               statusLedLevelHigh(enabled, TIME_AUTHORITY_LED_ACTIVE_LOW)
                   ? HIGH
                   : LOW);
}

inline void beginEsp8266StatusLeds() {
  setStatusLed(false);
  pinMode(PIN_STATUS_LED, OUTPUT);
  setTimeAuthorityLed(false);
  pinMode(PIN_TIME_AUTHORITY_LED, OUTPUT);
}
#endif
