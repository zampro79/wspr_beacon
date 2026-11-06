#pragma once

#include <stddef.h>
#include <stdint.h>

enum class NmeaSentenceType : uint8_t { NONE, GGA, RMC };

struct NmeaUpdate {
  NmeaSentenceType type = NmeaSentenceType::NONE;
  bool timeValid = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  bool positionValid = false;
  float latitude = 0.0f;
  float longitude = 0.0f;
  uint8_t satellites = 0;
  float altitude = 0.0f;
};

class NmeaLineBuffer {
 public:
  static constexpr size_t CAPACITY = 96;

  bool push(char value);
  char *sentence() { return buffer_; }
  void reset() { length_ = 0; }

 private:
  char buffer_[CAPACITY] = {};
  size_t length_ = 0;
};

class NmeaParser {
 public:
  static bool parse(char *sentence, NmeaUpdate &update);

 private:
  static bool validChecksum(const char *sentence);
  static bool parseTime(const char *value, NmeaUpdate &update);
  static bool parseCoordinate(const char *value, char hemisphere, float &result);
};
