#include "WifiCredentials.h"

#include <string.h>

namespace {
bool printable(const char *value, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (character < 32 || character > 126) return false;
  }
  return true;
}

bool hexadecimal(const char *value, size_t length) {
  for (size_t index = 0; index < length; ++index) {
    const char character = value[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'A' && character <= 'F') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool WifiCredentials::validSsid(const char *value) {
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  return length >= 1 && length < SSID_CAPACITY && printable(value, length);
}

bool WifiCredentials::validPassword(const char *value) {
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  if (length == 64) return hexadecimal(value, length);
  return length >= 8 && length <= 63 && printable(value, length);
}

bool WifiCredentials::setSsid(const char *value) {
  if (!validSsid(value)) return false;
  strcpy(ssid, value);
  return true;
}

bool WifiCredentials::setPassword(const char *value) {
  if (!validPassword(value)) return false;
  clearPassword();
  strcpy(password, value);
  open = false;
  return true;
}

void WifiCredentials::clearPassword() {
  volatile char *cursor = password;
  for (size_t index = 0; index < sizeof(password); ++index) cursor[index] = '\0';
}
