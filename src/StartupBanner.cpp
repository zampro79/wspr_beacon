#include "StartupBanner.h"

#include <string.h>

namespace {
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t TEXT_AREA_X = 38;
constexpr uint8_t TEXT_AREA_WIDTH = DISPLAY_WIDTH - TEXT_AREA_X;
constexpr uint8_t FONT_WIDTH = 6;
constexpr uint8_t FONT_HEIGHT = 8;
constexpr uint8_t LINE_Y[StartupBanner::LINE_COUNT] = {4, 25, 46};
const char *const LINE_TEXT[StartupBanner::LINE_COUNT] = {
    "IW2NNV-AC2WZ", "WSPR Beacon 2.0", "(2026)"};
}  // namespace

StartupBannerLine StartupBanner::line(uint8_t index) {
  if (index >= LINE_COUNT) return {nullptr, 0, 0, 0, 0};
  const uint8_t width = strlen(LINE_TEXT[index]) * FONT_WIDTH;
  return {LINE_TEXT[index],
          static_cast<uint8_t>(TEXT_AREA_X + (TEXT_AREA_WIDTH - width) / 2),
          LINE_Y[index], width, FONT_HEIGHT};
}

StartupBannerBounds StartupBanner::graphicBounds() {
  return {2, 2, 31, 57};
}

StartupBannerBounds StartupBanner::separatorBounds() {
  return {40, 18, 86, 1};
}
