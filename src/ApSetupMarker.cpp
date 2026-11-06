#include "ApSetupMarker.h"

#include <string.h>

#include "PersistentConfigRecord.h"

namespace {
constexpr uint8_t MAGIC[4] = {'A', 'P', 'S', 'U'};
constexpr uint8_t VERSION = 1;
constexpr uint8_t PENDING = 1;
constexpr uint8_t CONSUMED = 2;

void encode(uint8_t state, uint8_t data[ApSetupMarker::SIZE]) {
  memset(data, 0, ApSetupMarker::SIZE);
  memcpy(data, MAGIC, sizeof(MAGIC));
  data[4] = VERSION;
  data[5] = state;
  const uint32_t crc = PersistentConfigRecord::crc32(data, 12);
  for (uint8_t index = 0; index < 4; ++index)
    data[12 + index] = static_cast<uint8_t>(crc >> (index * 8));
}
}  // namespace

ApSetupMarkerState ApSetupMarker::read() const {
  uint8_t data[SIZE] = {};
  size_t length = 0;
  const StorageReadResult result = storage_.readApSetupMarker(data, sizeof(data), length);
  if (result == StorageReadResult::ABSENT) return ApSetupMarkerState::ABSENT;
  if (result != StorageReadResult::OK) return ApSetupMarkerState::STORAGE_ERROR;
  if (length != SIZE || memcmp(data, MAGIC, sizeof(MAGIC)) != 0 ||
      data[4] != VERSION || (data[5] != PENDING && data[5] != CONSUMED) ||
      data[6] != 0 || data[7] != 0 || data[8] != 0 || data[9] != 0 ||
      data[10] != 0 || data[11] != 0) return ApSetupMarkerState::CORRUPT;
  const uint32_t crc = PersistentConfigRecord::crc32(data, 12);
  for (uint8_t index = 0; index < 4; ++index)
    if (data[12 + index] != static_cast<uint8_t>(crc >> (index * 8)))
      return ApSetupMarkerState::CORRUPT;
  return data[5] == PENDING ? ApSetupMarkerState::PENDING
                            : ApSetupMarkerState::CONSUMED;
}

ApSetupMarkerResult ApSetupMarker::writeAndVerify(uint8_t state) {
  uint8_t data[SIZE];
  encode(state, data);
  if (!storage_.writeApSetupMarker(data, sizeof(data)))
    return ApSetupMarkerResult::STORAGE_ERROR;
  uint8_t verify[SIZE] = {};
  size_t length = 0;
  if (storage_.readApSetupMarker(verify, sizeof(verify), length) !=
          StorageReadResult::OK || length != SIZE ||
      memcmp(data, verify, SIZE) != 0)
    return ApSetupMarkerResult::VERIFICATION_FAILED;
  return ApSetupMarkerResult::OK;
}

ApSetupMarkerResult ApSetupMarker::request() {
  const ApSetupMarkerState state = read();
  if (state == ApSetupMarkerState::PENDING)
    return ApSetupMarkerResult::ALREADY_PENDING;
  if (state == ApSetupMarkerState::CORRUPT)
    return ApSetupMarkerResult::CORRUPT;
  if (state == ApSetupMarkerState::STORAGE_ERROR)
    return ApSetupMarkerResult::STORAGE_ERROR;
  return writeAndVerify(PENDING);
}

ApSetupMarkerResult ApSetupMarker::consume() {
  const ApSetupMarkerState state = read();
  if (state == ApSetupMarkerState::CORRUPT)
    return ApSetupMarkerResult::CORRUPT;
  if (state == ApSetupMarkerState::STORAGE_ERROR)
    return ApSetupMarkerResult::STORAGE_ERROR;
  if (state != ApSetupMarkerState::PENDING)
    return ApSetupMarkerResult::VERIFICATION_FAILED;
  return writeAndVerify(CONSUMED);
}

ApSetupMarkerResult ApSetupMarker::cancelForFactoryReset() {
  const ApSetupMarkerState state = read();
  if (state == ApSetupMarkerState::STORAGE_ERROR)
    return ApSetupMarkerResult::STORAGE_ERROR;
  if (state == ApSetupMarkerState::ABSENT ||
      state == ApSetupMarkerState::CONSUMED)
    return ApSetupMarkerResult::OK;
  // A corrupt AP marker has no trustworthy requested state. Overwrite it
  // while the independent Factory Reset marker still protects recovery.
  return writeAndVerify(CONSUMED);
}
