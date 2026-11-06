#include <unity.h>

#include "RfSafetyController.h"

class FakeRfDriver : public RfDriver {
 public:
  bool present = true;
  bool initializeResult = true;
  bool frequencyResult = true;
  bool outputEnabled = false;
  uint8_t detectionAttempts = 0;
  uint8_t initializationAttempts = 0;
  uint8_t disableCalls = 0;
  uint32_t referenceHz = 0;
  int32_t correction = 0;

  bool detected() override {
    ++detectionAttempts;
    return present;
  }
  bool initialize(uint32_t reference, int32_t correctionValue) override {
    ++initializationAttempts;
    referenceHz = reference;
    correction = correctionValue;
    return initializeResult;
  }
  bool setFrequency(uint64_t) override { return frequencyResult; }
  void enableOutput() override { outputEnabled = true; }
  void disableOutput() override {
    outputEnabled = false;
    ++disableCalls;
  }
};

void test_absent_device_inhibits_output() {
  FakeRfDriver driver;
  driver.present = false;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_FALSE(rf.begin());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RfFault::NOT_DETECTED),
                          static_cast<uint8_t>(rf.fault()));
  TEST_ASSERT_FALSE(rf.prepareFrequency(1409710000ULL));
  TEST_ASSERT_FALSE(rf.enableOutput());
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_calibration_changes_only_with_output_disabled() {
  FakeRfDriver driver;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_TRUE(rf.configure(26000000UL, -12345));
  TEST_ASSERT_TRUE(rf.begin());
  TEST_ASSERT_EQUAL_UINT32(26000000UL, driver.referenceHz);
  TEST_ASSERT_EQUAL_INT32(-12345, driver.correction);
  TEST_ASSERT_TRUE(rf.prepareFrequency(1409710000ULL));
  TEST_ASSERT_TRUE(rf.enableOutput());
  TEST_ASSERT_FALSE(rf.configure(25000000UL, 999));
  TEST_ASSERT_EQUAL_INT32(-12345, rf.correction());
}

void test_initialization_failure_inhibits_output() {
  FakeRfDriver driver;
  driver.initializeResult = false;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_FALSE(rf.begin());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RfFault::INITIALIZATION_FAILED),
      static_cast<uint8_t>(rf.fault()));
  TEST_ASSERT_FALSE(rf.enableOutput());
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_successful_initialization_allows_normal_output() {
  FakeRfDriver driver;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_TRUE(rf.begin());
  TEST_ASSERT_TRUE(rf.prepareFrequency(1409710000ULL));
  TEST_ASSERT_TRUE(rf.enableOutput());
  TEST_ASSERT_TRUE(rf.operational());
  TEST_ASSERT_TRUE(driver.outputEnabled);
  rf.disableOutput();
  TEST_ASSERT_FALSE(driver.outputEnabled);
}

void test_critical_error_during_frame_disables_output() {
  FakeRfDriver driver;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_TRUE(rf.begin());
  TEST_ASSERT_TRUE(rf.prepareFrequency(1409710000ULL));
  TEST_ASSERT_TRUE(rf.enableOutput());
  rf.reportCriticalError();
  TEST_ASSERT_FALSE(rf.operational());
  TEST_ASSERT_FALSE(rf.outputEnabled());
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RfFault::CRITICAL_RUNTIME_ERROR),
      static_cast<uint8_t>(rf.fault()));
}

void test_frequency_failure_aborts_and_latches_fault() {
  FakeRfDriver driver;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_TRUE(rf.begin());
  TEST_ASSERT_TRUE(rf.prepareFrequency(1409710000ULL));
  TEST_ASSERT_TRUE(rf.enableOutput());
  driver.frequencyResult = false;
  TEST_ASSERT_FALSE(rf.prepareFrequency(1409710146ULL));
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RfFault::FREQUENCY_CONFIGURATION_FAILED),
      static_cast<uint8_t>(rf.fault()));
}

void test_recovery_rechecks_presence_and_initialization() {
  FakeRfDriver driver;
  driver.present = false;
  RfSafetyController rf(driver, 25000000UL, 76000);
  TEST_ASSERT_FALSE(rf.begin());
  driver.present = true;
  TEST_ASSERT_TRUE(rf.recover());
  TEST_ASSERT_TRUE(rf.operational());
  TEST_ASSERT_EQUAL_UINT8(2, driver.detectionAttempts);
  TEST_ASSERT_EQUAL_UINT8(1, driver.initializationAttempts);
  TEST_ASSERT_FALSE(driver.outputEnabled);
  TEST_ASSERT_FALSE(rf.enableOutput());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_absent_device_inhibits_output);
  RUN_TEST(test_initialization_failure_inhibits_output);
  RUN_TEST(test_successful_initialization_allows_normal_output);
  RUN_TEST(test_critical_error_during_frame_disables_output);
  RUN_TEST(test_frequency_failure_aborts_and_latches_fault);
  RUN_TEST(test_recovery_rechecks_presence_and_initialization);
  RUN_TEST(test_calibration_changes_only_with_output_disabled);
  return UNITY_END();
}
