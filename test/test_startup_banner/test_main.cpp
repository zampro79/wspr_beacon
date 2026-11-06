#include <unity.h>

#include <cstring>

#include "StartupBanner.h"
#include "StartupController.h"
#include "WsprScheduler.h"

void setUp() {}
void tearDown() {}

void test_banner_text_dimensions_and_centering() {
  const char *expected[] = {"IW2NNV-AC2WZ", "WSPR Beacon 2.0", "(2026)"};
  const uint8_t expectedWidths[] = {72, 90, 36};
  const uint8_t expectedX[] = {47, 38, 65};
  const uint8_t expectedY[] = {4, 25, 46};

  for (uint8_t index = 0; index < StartupBanner::LINE_COUNT; ++index) {
    const StartupBannerLine line = StartupBanner::line(index);
    TEST_ASSERT_EQUAL_STRING(expected[index], line.text);
    TEST_ASSERT_EQUAL_UINT8(expectedWidths[index], line.width);
    TEST_ASSERT_EQUAL_UINT8(expectedX[index], line.x);
    TEST_ASSERT_EQUAL_UINT8(expectedY[index], line.y);
    TEST_ASSERT_EQUAL_UINT8(8, line.height);
    TEST_ASSERT_EQUAL_UINT8(90 - line.width - (line.x - 38), line.x - 38);
    TEST_ASSERT_TRUE(line.y + line.height <= 64);
  }
}

void test_graphic_text_and_separator_fit_without_overlap() {
  const StartupBannerBounds graphic = StartupBanner::graphicBounds();
  const StartupBannerBounds separator = StartupBanner::separatorBounds();

  TEST_ASSERT_EQUAL_UINT8(2, graphic.x);
  TEST_ASSERT_EQUAL_UINT8(2, graphic.y);
  TEST_ASSERT_EQUAL_UINT8(31, graphic.width);
  TEST_ASSERT_EQUAL_UINT8(57, graphic.height);
  TEST_ASSERT_TRUE(graphic.x + graphic.width <= 38);
  TEST_ASSERT_TRUE(graphic.y + graphic.height <= 64);
  TEST_ASSERT_TRUE(separator.x >= 38);
  TEST_ASSERT_TRUE(separator.x + separator.width <= 128);
  TEST_ASSERT_TRUE(separator.y + separator.height <= 64);

  for (uint8_t index = 0; index < StartupBanner::LINE_COUNT; ++index) {
    const StartupBannerLine line = StartupBanner::line(index);
    TEST_ASSERT_TRUE(line.x >= 38);
    TEST_ASSERT_TRUE(line.x + line.width <= 128);
    TEST_ASSERT_FALSE(line.y < separator.y + separator.height &&
                      line.y + line.height > separator.y);
  }
}

void test_startup_duration_is_non_blocking_and_wrap_safe() {
  StartupController startup;
  TEST_ASSERT_FALSE(startup.active(0));

  startup.begin(1000);
  TEST_ASSERT_TRUE(startup.active(1000));
  TEST_ASSERT_FALSE(startup.transmissionAllowed(5999));
  TEST_ASSERT_FALSE(startup.active(6000));
  TEST_ASSERT_TRUE(startup.transmissionAllowed(6000));

  startup.begin(UINT32_MAX - 1000UL);
  TEST_ASSERT_TRUE(startup.active(3998));
  TEST_ASSERT_FALSE(startup.active(3999));
}

void test_startup_is_independent_from_oled_and_does_not_recover_missed_slot() {
  const bool oledAvailable = false;
  StartupController startup;
  startup.begin(0);
  TEST_ASSERT_FALSE(oledAvailable);
  TEST_ASSERT_TRUE(startup.active(4999));

  WsprScheduler scheduler;
  TimeService timeService(TimeServiceConfig(5000UL, 3600000UL));
  TimeReference reference;
  reference.utcMillisOfDay = 5000UL;
  reference.quality = TimeQuality::PRECISE;
  reference.utcValid = true;
  TEST_ASSERT_TRUE(timeService.update(TimeSource::GPS, reference, 5000UL));
  const TimeSnapshot time = timeService.snapshot(5000UL);
  TEST_ASSERT_TRUE(WsprScheduler::transmissionAuthorized(time));
  TEST_ASSERT_FALSE(startup.transmissionAllowed(4999));
  TEST_ASSERT_FALSE(scheduler.armed());

  TEST_ASSERT_TRUE(startup.transmissionAllowed(5000));
  TEST_ASSERT_FALSE(scheduler.arm(time, 5000UL, 2, 10));
  TEST_ASSERT_FALSE(scheduler.armed());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_banner_text_dimensions_and_centering);
  RUN_TEST(test_graphic_text_and_separator_fit_without_overlap);
  RUN_TEST(test_startup_duration_is_non_blocking_and_wrap_safe);
  RUN_TEST(
      test_startup_is_independent_from_oled_and_does_not_recover_missed_slot);
  return UNITY_END();
}
