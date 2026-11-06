#pragma once

#include <stddef.h>

// Keeps each transport service step bounded to one TCP-MSS-sized submission.
// ESP8266 exposes usable send capacity; ESP32's public WiFiClient API does not.
class HttpWritePlanner {
 public:
  static constexpr size_t MAX_CHUNK_BYTES = 536U;

  static size_t plannedBytes(size_t remainingBytes, int availableBytes,
                             bool capacityIsMeaningful) {
    const size_t chunk = remainingBytes < MAX_CHUNK_BYTES
                             ? remainingBytes
                             : MAX_CHUNK_BYTES;
    if (!capacityIsMeaningful) return chunk;
    if (availableBytes <= 0) return 0;
    const size_t capacity = static_cast<size_t>(availableBytes);
    return capacity < chunk ? capacity : chunk;
  }
};
