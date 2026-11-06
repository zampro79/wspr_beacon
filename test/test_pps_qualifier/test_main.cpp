#include <unity.h>

#include "PpsQualifier.h"

namespace {
void qualify(PpsQualifier &qualifier, uint32_t first) {
  qualifier.noteEdge(first);
  qualifier.noteEdge(first + 1000UL);
  qualifier.noteEdge(first + 2000UL);
  qualifier.noteEdge(first + 3000UL);
}

void test_absent_and_first_edge_are_not_qualified() {
  PpsQualifier qualifier;
  TEST_ASSERT_FALSE(qualifier.qualified());
  TEST_ASSERT_EQUAL(
      static_cast<int>(PpsQualifier::EdgeResult::CANDIDATE),
      static_cast<int>(qualifier.noteEdge(1000UL)));
  TEST_ASSERT_FALSE(qualifier.qualified());
  TEST_ASSERT_EQUAL_UINT8(0, qualifier.validPeriods());
}

void test_three_consecutive_periods_are_required() {
  PpsQualifier qualifier;
  qualifier.noteEdge(1000UL);
  qualifier.noteEdge(1900UL);
  qualifier.noteEdge(3000UL);
  TEST_ASSERT_FALSE(qualifier.qualified());
  qualifier.noteEdge(4000UL);
  TEST_ASSERT_TRUE(qualifier.qualified());
  TEST_ASSERT_EQUAL_UINT8(PpsQualifier::REQUIRED_VALID_PERIODS,
                          qualifier.validPeriods());
}

void test_glitch_and_wrong_period_restart_full_sequence() {
  PpsQualifier qualifier;
  qualifier.noteEdge(1000UL);
  qualifier.noteEdge(2000UL);
  qualifier.noteEdge(2050UL);
  TEST_ASSERT_FALSE(qualifier.qualified());
  TEST_ASSERT_EQUAL_UINT8(0, qualifier.validPeriods());
  qualifier.noteEdge(3050UL);
  qualifier.noteEdge(4050UL);
  qualifier.noteEdge(5200UL);
  TEST_ASSERT_FALSE(qualifier.qualified());
  TEST_ASSERT_EQUAL_UINT8(0, qualifier.validPeriods());
}

void test_missing_edge_revokes_and_recovery_needs_full_sequence() {
  PpsQualifier qualifier;
  qualify(qualifier, 1000UL);
  TEST_ASSERT_TRUE(qualifier.qualified());
  TEST_ASSERT_TRUE(qualifier.expire(5101UL));
  TEST_ASSERT_FALSE(qualifier.qualified());
  qualifier.noteEdge(6000UL);
  qualifier.noteEdge(7000UL);
  qualifier.noteEdge(8000UL);
  TEST_ASSERT_FALSE(qualifier.qualified());
  qualifier.noteEdge(9000UL);
  TEST_ASSERT_TRUE(qualifier.qualified());
}

void test_multiple_unconsumed_edges_are_rejected() {
  PpsQualifier qualifier;
  qualify(qualifier, 1000UL);
  TEST_ASSERT_TRUE(qualifier.qualified());
  TEST_ASSERT_EQUAL(
      static_cast<int>(PpsQualifier::EdgeResult::REJECTED),
      static_cast<int>(qualifier.noteEdge(4050UL, 2)));
  TEST_ASSERT_FALSE(qualifier.qualified());
  TEST_ASSERT_TRUE(qualifier.hasCandidate());
}

void test_unsigned_timestamp_overflow_preserves_period_measurement() {
  PpsQualifier qualifier;
  const uint32_t first = UINT32_MAX - 2500UL;
  qualify(qualifier, first);
  TEST_ASSERT_TRUE(qualifier.qualified());
  TEST_ASSERT_EQUAL_UINT32(first + 3000UL, qualifier.lastEdgeMillis());
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_absent_and_first_edge_are_not_qualified);
  RUN_TEST(test_three_consecutive_periods_are_required);
  RUN_TEST(test_glitch_and_wrong_period_restart_full_sequence);
  RUN_TEST(test_missing_edge_revokes_and_recovery_needs_full_sequence);
  RUN_TEST(test_multiple_unconsumed_edges_are_rejected);
  RUN_TEST(test_unsigned_timestamp_overflow_preserves_period_measurement);
  return UNITY_END();
}
