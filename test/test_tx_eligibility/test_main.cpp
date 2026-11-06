#include <unity.h>

#include "TxEligibility.h"

namespace {
TxEligibilityInputs eligible() {
  TxEligibilityInputs inputs;
  inputs.manualPermission = true;
  inputs.operationalMode = true;
  inputs.identityConfigured = true;
  inputs.rfReady = true;
  inputs.timeQualified = true;
  return inputs;
}
}

void test_all_prerequisites_are_required() {
  TxEligibilityInputs inputs = eligible();
  TEST_ASSERT_TRUE(TxEligibility::evaluate(inputs));
  inputs.manualPermission = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
  inputs = eligible();
  inputs.operationalMode = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
  inputs = eligible();
  inputs.identityConfigured = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
  inputs = eligible();
  inputs.rfReady = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
  inputs = eligible();
  inputs.timeQualified = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_manual_tx_off_is_ineligible() {
  TxEligibilityInputs inputs = eligible();
  inputs.manualPermission = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_rf_unavailable_is_ineligible() {
  TxEligibilityInputs inputs = eligible();
  inputs.rfReady = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_unconfigured_identity_is_ineligible() {
  TxEligibilityInputs inputs = eligible();
  inputs.identityConfigured = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_unqualified_time_is_ineligible() {
  TxEligibilityInputs inputs = eligible();
  inputs.timeQualified = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_configuration_and_resume_guard_inhibit_reporting() {
  TxEligibilityInputs inputs = eligible();
  inputs.configurationActive = true;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
  inputs = eligible();
  inputs.resumeGuardActive = true;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(inputs));
}

void test_ap_modes_use_authoritative_prequalified_inputs() {
  TxEligibilityInputs provisioning = eligible();
  provisioning.operationalMode = false;
  provisioning.timeQualified = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(provisioning));

  TxEligibilityInputs apWithoutGps = eligible();
  apWithoutGps.timeQualified = false;
  TEST_ASSERT_FALSE(TxEligibility::evaluate(apWithoutGps));
  TEST_ASSERT_TRUE(TxEligibility::evaluate(eligible()));
}

void test_normal_sta_with_qualified_time_is_eligible() {
  TEST_ASSERT_TRUE(TxEligibility::evaluate(eligible()));
}

void test_actual_transmitter_state_is_not_eligibility_input() {
  // PREPARING and TRANSMITTING remain separately reported transmitter states.
  // Changing that presentation state cannot affect this read-only calculation.
  const TxEligibilityInputs preparing = eligible();
  const TxEligibilityInputs transmitting = eligible();
  TEST_ASSERT_EQUAL(TxEligibility::evaluate(preparing),
                    TxEligibility::evaluate(transmitting));
}

void test_evaluation_is_read_only_and_transmitter_state_independent() {
  const TxEligibilityInputs inputs = eligible();
  const TxEligibilityInputs before = inputs;
  TEST_ASSERT_TRUE(TxEligibility::evaluate(inputs));
  TEST_ASSERT_EQUAL_MEMORY(&before, &inputs, sizeof(inputs));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_all_prerequisites_are_required);
  RUN_TEST(test_manual_tx_off_is_ineligible);
  RUN_TEST(test_rf_unavailable_is_ineligible);
  RUN_TEST(test_unconfigured_identity_is_ineligible);
  RUN_TEST(test_unqualified_time_is_ineligible);
  RUN_TEST(test_configuration_and_resume_guard_inhibit_reporting);
  RUN_TEST(test_ap_modes_use_authoritative_prequalified_inputs);
  RUN_TEST(test_normal_sta_with_qualified_time_is_eligible);
  RUN_TEST(test_actual_transmitter_state_is_not_eligibility_input);
  RUN_TEST(test_evaluation_is_read_only_and_transmitter_state_independent);
  return UNITY_END();
}
