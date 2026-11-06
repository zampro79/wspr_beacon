#include "NmeaParser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool NmeaLineBuffer::push(char value) {
  if (value == '$') {
    length_ = 0;
    buffer_[length_++] = value;
    return false;
  }
  if (length_ == 0 || value == '\r') return false;
  if (value == '\n') {
    buffer_[length_] = '\0';
    return true;
  }
  if (length_ < CAPACITY - 1) {
    buffer_[length_++] = value;
  } else {
    length_ = 0;
  }
  return false;
}

bool NmeaParser::validChecksum(const char *sentence) {
  if (!sentence || sentence[0] != '$') return false;
  const char *star = strchr(sentence, '*');
  if (!star || !isxdigit(static_cast<unsigned char>(star[1])) ||
      !isxdigit(static_cast<unsigned char>(star[2]))) return false;
  uint8_t checksum = 0;
  for (const char *cursor = sentence + 1; cursor < star; ++cursor) checksum ^= *cursor;
  return checksum == static_cast<uint8_t>(strtoul(star + 1, nullptr, 16));
}

bool NmeaParser::parseTime(const char *value, NmeaUpdate &update) {
  if (!value || strlen(value) < 6) return false;
  for (uint8_t i = 0; i < 6; ++i) {
    if (!isdigit(static_cast<unsigned char>(value[i]))) return false;
  }
  const int hour = (value[0] - '0') * 10 + value[1] - '0';
  const int minute = (value[2] - '0') * 10 + value[3] - '0';
  const int second = (value[4] - '0') * 10 + value[5] - '0';
  if (hour > 23 || minute > 59 || second > 60) return false;
  update.hour = static_cast<uint8_t>(hour);
  update.minute = static_cast<uint8_t>(minute);
  update.second = static_cast<uint8_t>(second == 60 ? 59 : second);
  update.timeValid = true;
  return true;
}

bool NmeaParser::parseCoordinate(const char *value, char hemisphere, float &result) {
  if (!value || !*value) return false;
  if (hemisphere != 'N' && hemisphere != 'S' && hemisphere != 'E' && hemisphere != 'W') return false;
  const float raw = static_cast<float>(atof(value));
  const int degrees = static_cast<int>(raw / 100.0f);
  const float minutes = raw - degrees * 100.0f;
  if (minutes < 0.0f || minutes >= 60.0f) return false;
  result = degrees + minutes / 60.0f;
  if ((hemisphere == 'N' || hemisphere == 'S') && result > 90.0f) return false;
  if ((hemisphere == 'E' || hemisphere == 'W') && result > 180.0f) return false;
  if (hemisphere == 'S' || hemisphere == 'W') result = -result;
  return true;
}

bool NmeaParser::parse(char *sentence, NmeaUpdate &update) {
  update = NmeaUpdate{};
  if (!validChecksum(sentence)) return false;
  char *star = strchr(sentence, '*');
  *star = '\0';

  char *fields[16] = {};
  uint8_t count = 0;
  char *cursor = sentence + 1;
  while (cursor && count < 16) {
    fields[count++] = cursor;
    char *comma = strchr(cursor, ',');
    if (!comma) break;
    *comma = '\0';
    cursor = comma + 1;
  }
  if (count == 0 || strlen(fields[0]) < 5) return false;
  const char *type = fields[0] + strlen(fields[0]) - 3;

  if (strcmp(type, "GGA") == 0) {
    if (count < 10) return false;
    update.type = NmeaSentenceType::GGA;
    parseTime(fields[1], update);
    const int quality = atoi(fields[6]);
    update.satellites = static_cast<uint8_t>(atoi(fields[7]));
    update.altitude = static_cast<float>(atof(fields[9]));
    update.positionValid = quality > 0 &&
        parseCoordinate(fields[2], fields[3][0], update.latitude) &&
        parseCoordinate(fields[4], fields[5][0], update.longitude);
    return true;
  }

  if (strcmp(type, "RMC") == 0) {
    if (count < 7) return false;
    update.type = NmeaSentenceType::RMC;
    parseTime(fields[1], update);
    update.positionValid = fields[2][0] == 'A' &&
        parseCoordinate(fields[3], fields[4][0], update.latitude) &&
        parseCoordinate(fields[5], fields[6][0], update.longitude);
    return true;
  }

  return false;
}
