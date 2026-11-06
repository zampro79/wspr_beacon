#include "PlatformEntropy.h"

#include <Arduino.h>

#if defined(ESP32)
#include <esp_system.h>
#elif defined(ESP8266)
extern "C" {
#include <user_interface.h>
}
#endif

uint32_t platformEntropySeed() {
#if defined(ESP8266)
  return ESP.random();
#elif defined(ESP32)
  return esp_random();
#else
  return micros() ^ (static_cast<uint32_t>(analogRead(A0)) << 16);
#endif
}

bool platformFillRandom(void *, uint8_t *destination, size_t length) {
  if (destination == nullptr) return false;
#if defined(ESP8266)
  return os_get_random(destination, length) == 0;
#elif defined(ESP32)
  esp_fill_random(destination, length);
  return true;
#else
  (void)length;
  return false;
#endif
}
