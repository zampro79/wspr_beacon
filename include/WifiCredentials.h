#pragma once

#include <stddef.h>
#include <stdint.h>

struct WifiCredentials {
  static constexpr size_t SSID_CAPACITY = 33;
  static constexpr size_t PASSWORD_CAPACITY = 65;

  char ssid[SSID_CAPACITY] = {};
  char password[PASSWORD_CAPACITY] = {};
  bool open = false;

  bool setSsid(const char *value);
  bool setPassword(const char *value);
  bool hasSsid() const { return ssid[0] != '\0'; }
  bool hasPassword() const { return password[0] != '\0'; }
  bool complete() const { return hasSsid() && (open || hasPassword()); }
  void clearPassword();

  static bool validSsid(const char *value);
  static bool validPassword(const char *value);
};
