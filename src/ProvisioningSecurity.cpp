#include "ProvisioningSecurity.h"

bool ProvisioningSecurity::generateNumericPassword(
    FillRandom fill, void *context,
    char destination[PASSWORD_DIGITS + 1U]) {
  if (fill == nullptr || destination == nullptr) return false;
  for (size_t index = 0; index < PASSWORD_DIGITS; ++index) {
    uint8_t value = 0;
    do {
      if (!fill(context, &value, 1U)) {
        clear(destination, PASSWORD_DIGITS + 1U);
        return false;
      }
    } while (value >= 250U);
    destination[index] = static_cast<char>('0' + value % 10U);
  }
  destination[PASSWORD_DIGITS] = '\0';
  return true;
}

void ProvisioningSecurity::clear(void *value, size_t length) {
  volatile uint8_t *cursor = static_cast<volatile uint8_t *>(value);
  while (length-- != 0U) *cursor++ = 0;
}
