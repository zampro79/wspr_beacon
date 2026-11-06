#pragma once

constexpr bool statusLedLevelHigh(bool enabled, bool activeLow) {
  return enabled != activeLow;
}
