#include <unity.h>

#include <string.h>

#include "ApSetupBootDecision.h"
#include "ApSetupMarker.h"
#include "BootModeController.h"
#include "PersistentConfigRecord.h"

namespace {
class FakeStorage : public ConfigStorage {
 public:
  uint8_t marker[ApSetupMarker::SIZE] = {};
  size_t length = 0;
  bool present = false;
  bool readError = false;
  bool writeError = false;
  bool corruptWrite = false;
  bool partialWrite = false;
  unsigned wifiWrites = 0;

  bool read(ConfigRecordType, uint8_t, uint8_t *, size_t, size_t &size) override {
    size = 0; return false;
  }
  bool write(ConfigRecordType type, uint8_t, const uint8_t *, size_t) override {
    if (type == ConfigRecordType::WIFI) ++wifiWrites;
    return true;
  }
  StorageReadResult readApSetupMarker(uint8_t *data, size_t capacity,
                                      size_t &size) override {
    size = 0;
    if (readError) return StorageReadResult::ERROR;
    if (!present) return StorageReadResult::ABSENT;
    if (length > capacity) return StorageReadResult::ERROR;
    memcpy(data, marker, length);
    size = length;
    return StorageReadResult::OK;
  }
  bool writeApSetupMarker(const uint8_t *data, size_t size) override {
    if (writeError) return false;
    present = true;
    length = partialWrite ? size / 2 : size;
    memcpy(marker, data, length);
    if (corruptWrite) marker[0] ^= 1;
    return true;
  }
};

void test_absent_valid_and_consumed() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::ABSENT, marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::ALREADY_PENDING, marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.consume());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.cancelForFactoryReset());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, marker.read());
  TEST_ASSERT_EQUAL_UINT(0, storage.wifiWrites);
}

void test_corruption_and_version() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.request());
  storage.marker[4] = 2;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::CORRUPT, marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::CORRUPT, marker.consume());
  storage.marker[4] = 1;
  storage.marker[8] ^= 1;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, marker.read());
}

void test_storage_errors_and_write_verification() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  storage.readError = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::STORAGE_ERROR, marker.read());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::STORAGE_ERROR, marker.request());
  storage.readError = false;
  storage.writeError = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::STORAGE_ERROR, marker.request());
  storage.writeError = false;
  storage.corruptWrite = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::VERIFICATION_FAILED, marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, marker.read());
}

void test_interrupted_write_and_consume_failure() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  storage.partialWrite = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::VERIFICATION_FAILED, marker.request());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, marker.read());
  FakeStorage second;
  ApSetupMarker pending(second);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, pending.request());
  second.writeError = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::STORAGE_ERROR, pending.consume());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, pending.read());
  second.writeError = false;
  second.corruptWrite = true;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::VERIFICATION_FAILED, pending.consume());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CORRUPT, pending.read());
  second.corruptWrite = false;
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK,
                        pending.cancelForFactoryReset());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::CONSUMED, pending.read());
}

void test_boot_decision_and_power_loss() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  const auto normal = FactoryResetBootState::NORMAL;
  TEST_ASSERT_TRUE(ApSetupBootDecision::trySta(true, true, normal, marker.read()));
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.request());
  ApSetupMarker afterPowerLoss(storage);
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(true, true, normal,
                                               afterPowerLoss.read()));
  BootModeController boot;
  boot.begin(false, 0);
  TEST_ASSERT_TRUE(boot.enterApRequested());
  TEST_ASSERT_FALSE(boot.rfOperationAllowed());
  boot.apStartResult(false);
  TEST_ASSERT_TRUE(boot.failClosed());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerState::PENDING, marker.read());
  boot.begin(false, 0);
  boot.apStartResult(true);
  TEST_ASSERT_TRUE(boot.provisioning());
  TEST_ASSERT_FALSE(boot.rfOperationAllowed());
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.consume());
  ApSetupMarker afterSecondPowerLoss(storage);
  TEST_ASSERT_TRUE(ApSetupBootDecision::trySta(true, true, normal,
                                              afterSecondPowerLoss.read()));
  TEST_ASSERT_EQUAL_UINT(0, storage.wifiWrites);
}

void test_factory_reset_priority_and_fail_closed() {
  FakeStorage storage;
  ApSetupMarker marker(storage);
  TEST_ASSERT_EQUAL_INT(ApSetupMarkerResult::OK, marker.request());
  TEST_ASSERT_TRUE(ApSetupBootDecision::trySta(
      true, true, FactoryResetBootState::COMPLETED, marker.read()));
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(
      true, true, FactoryResetBootState::FAIL_CLOSED, marker.read()));
  storage.marker[0] ^= 1;
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(
      true, true, FactoryResetBootState::NORMAL, marker.read()));
  storage.readError = true;
  TEST_ASSERT_FALSE(ApSetupBootDecision::trySta(
      true, true, FactoryResetBootState::NORMAL, marker.read()));
}
}  // namespace

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_absent_valid_and_consumed);
  RUN_TEST(test_corruption_and_version);
  RUN_TEST(test_storage_errors_and_write_verification);
  RUN_TEST(test_interrupted_write_and_consume_failure);
  RUN_TEST(test_boot_decision_and_power_loss);
  RUN_TEST(test_factory_reset_priority_and_fail_closed);
  return UNITY_END();
}
