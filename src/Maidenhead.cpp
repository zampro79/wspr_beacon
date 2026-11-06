#include "Maidenhead.h"

bool maidenhead4(float latitude, float longitude, char output[5]) {
  if (!output || latitude < -90.0f || latitude > 90.0f ||
      longitude < -180.0f || longitude > 180.0f) return false;

  longitude += 180.0f;
  latitude += 90.0f;
  int longitudeField = static_cast<int>(longitude / 20.0f);
  int latitudeField = static_cast<int>(latitude / 10.0f);
  const int longitudeSquare = static_cast<int>(longitude / 2.0f) % 10;
  const int latitudeSquare = static_cast<int>(latitude) % 10;
  if (longitudeField > 17) longitudeField = 17;
  if (latitudeField > 17) latitudeField = 17;

  output[0] = static_cast<char>('A' + longitudeField);
  output[1] = static_cast<char>('A' + latitudeField);
  output[2] = static_cast<char>('0' + longitudeSquare);
  output[3] = static_cast<char>('0' + latitudeSquare);
  output[4] = '\0';
  return true;
}
