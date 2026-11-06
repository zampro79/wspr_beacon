#pragma once

#include <stdint.h>

struct StartupBannerLine {
  const char *text;
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
};

struct StartupBannerBounds {
  uint8_t x;
  uint8_t y;
  uint8_t width;
  uint8_t height;
};

class StartupBanner {
 public:
  static constexpr uint8_t LINE_COUNT = 3;

  static StartupBannerLine line(uint8_t index);
  static StartupBannerBounds graphicBounds();
  static StartupBannerBounds separatorBounds();
};
