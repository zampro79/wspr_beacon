#include <unity.h>

#include <string.h>

#include "ConfigurationManager.h"
#include "PersistentConfigRecord.h"

namespace {
const uint8_t WSPR_V1_FIXTURE[PersistentConfigRecord::WSPR_RECORD_SIZE] = {
    0x57, 0x53, 0x50, 0x52, 0x01, 0x01, 0x01, 0x00,
    0x16, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x85, 0xA8,
    0xCD, 0xB6, 0x49, 0x5A, 0x32, 0x41, 0x42, 0x43,
    0x4A, 0x4E, 0x34, 0x35, 0xCC, 0x1A, 0xD7, 0x00,
    0xE7, 0xFF, 0xE0, 0x28, 0x01, 0x00, 0x17, 0x04};

void put32(uint8_t *data, uint32_t value) {
  for (uint8_t index = 0; index < 4; ++index) {
    data[index] = static_cast<uint8_t>(value >> (index * 8));
  }
}

void refreshRecordCrc(uint8_t *record) {
  uint8_t covered[14 + PersistentConfigRecord::WSPR_PAYLOAD_SIZE];
  memcpy(covered, record, 14);
  memcpy(covered + 14, record + PersistentConfigRecord::HEADER_SIZE,
         PersistentConfigRecord::WSPR_PAYLOAD_SIZE);
  put32(record + 14,
        PersistentConfigRecord::crc32(covered, sizeof(covered)));
}

class FakeStorage : public ConfigStorage {
 public:
  struct Slot {
    uint8_t data[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
    size_t length = 0;
    bool present = false;
  };

  bool read(ConfigRecordType type, uint8_t slot, uint8_t *data,
            size_t capacity, size_t &length) override {
    if (failReads) return false;
    const Slot &source = slots[index(type)][slot];
    if (!source.present || source.length > capacity) return false;
    memcpy(data, source.data, source.length);
    length = source.length;
    return true;
  }
  bool write(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) override {
    ++writes[index(type)];
    if (failType == type) return false;
    Slot &target = slots[index(type)][slot];
    target.present = true;
    target.length = interruptedBytes != 0 && interruptedBytes < length
                        ? interruptedBytes
                        : length;
    memcpy(target.data, data, target.length);
    interruptedBytes = 0;
    return true;
  }
  void place(ConfigRecordType type, uint8_t slot, const uint8_t *data,
             size_t length) {
    Slot &target = slots[index(type)][slot];
    target.present = true;
    target.length = length;
    memcpy(target.data, data, length);
  }

  Slot slots[2][2];
  uint16_t writes[2] = {};
  ConfigRecordType failType = static_cast<ConfigRecordType>(0);
  size_t interruptedBytes = 0;
  bool failReads = false;

 private:
  static uint8_t index(ConfigRecordType type) {
    return type == ConfigRecordType::WSPR ? 0 : 1;
  }
};

BeaconSettings wsprWithCallsign(const char *callsign) {
  BeaconSettings settings;
  strcpy(settings.callsign, callsign);
  return settings;
}
WifiCredentials wifi(const char *ssid, const char *password) {
  WifiCredentials credentials;
  TEST_ASSERT_TRUE(credentials.setSsid(ssid));
  TEST_ASSERT_TRUE(credentials.setPassword(password));
  return credentials;
}
void test_open_wifi_record_uses_v2_and_old_protected_record_still_loads() {
  uint8_t encoded[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  WifiCredentials open;
  TEST_ASSERT_TRUE(open.setSsid("Guest"));
  open.open = true;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      open, 3, encoded, sizeof(encoded), length));
  TEST_ASSERT_EQUAL_UINT8(PersistentConfigRecord::WIFI_PAYLOAD_VERSION_V2,
                          encoded[6]);
  WifiCredentials decoded;
  uint32_t sequence = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::decodeWifi(
      encoded, length, decoded, sequence));
  TEST_ASSERT_TRUE(decoded.open);
  TEST_ASSERT_TRUE(decoded.complete());
  TEST_ASSERT_EQUAL_STRING("Guest", decoded.ssid);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("Home", "12345678"), 4, encoded, sizeof(encoded), length));
  TEST_ASSERT_EQUAL_UINT8(PersistentConfigRecord::WIFI_PAYLOAD_VERSION,
                          encoded[6]);
  TEST_ASSERT_TRUE(PersistentConfigRecord::decodeWifi(
      encoded, length, decoded, sequence));
  TEST_ASSERT_FALSE(decoded.open);
  TEST_ASSERT_EQUAL_STRING("12345678", decoded.password);
}
void save(ConfigurationManager &manager) {
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
                          static_cast<uint8_t>(manager.processPending(false)));
}
bool containsBytes(const void *data, size_t length, const char *text) {
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  const size_t textLength = strlen(text);
  for (size_t index = 0; index + textLength <= length; ++index) {
    if (memcmp(bytes + index, text, textLength) == 0) return true;
  }
  return false;
}
}  // namespace

void test_v3_round_trip_preserves_all_fields_and_crc() {
  const uint8_t known[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL,
                          PersistentConfigRecord::crc32(known, sizeof(known)));
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings source = wsprWithCallsign("IZ2ABC");
  strcpy(source.fixedLocator, "AA00");
  source.baseFrequencyHz = 14097100UL;
  source.frequencyOffsetHz = -90;
  source.si5351Correction = -12345;
  source.powerDbm = 37;
  source.transmitIntervalMinutes = 6;
  source.band = WsprBandId::BAND_20M;
  source.frequencyMode = WsprFrequencyMode::RANDOM;
  source.powerSave = true;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      source, 42, record, sizeof(record), length));
  BeaconSettings decoded;
  uint32_t sequence = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::decodeWspr(
      record, length, decoded, sequence));
  TEST_ASSERT_EQUAL_UINT32(42, sequence);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", decoded.callsign);
  TEST_ASSERT_EQUAL_STRING("AA00", decoded.fixedLocator);
  TEST_ASSERT_EQUAL_UINT32(source.baseFrequencyHz, decoded.baseFrequencyHz);
  TEST_ASSERT_EQUAL_INT16(source.frequencyOffsetHz, decoded.frequencyOffsetHz);
  TEST_ASSERT_EQUAL_INT32(source.si5351Correction, decoded.si5351Correction);
  TEST_ASSERT_EQUAL_UINT8(source.powerDbm, decoded.powerDbm);
  TEST_ASSERT_EQUAL_UINT8(source.transmitIntervalMinutes,
                          decoded.transmitIntervalMinutes);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(source.band),
                          static_cast<uint8_t>(decoded.band));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(source.frequencyMode),
                          static_cast<uint8_t>(decoded.frequencyMode));
  TEST_ASSERT_TRUE(decoded.powerSave);
  record[length - 1] ^= 0x01;
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWspr(
      record, length, decoded, sequence));
}

void test_v3_round_trip_preserves_every_band_mode_and_power_save() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    for (uint8_t random = 0; random < 2; ++random) {
      BeaconSettings source = wsprWithCallsign("IW2NNV");
      source.band = band->id;
      source.baseFrequencyHz = band->nominalToneZeroHz;
      source.frequencyMode = random ? WsprFrequencyMode::RANDOM
                                    : WsprFrequencyMode::FIXED;
      source.powerSave = random != 0;
      uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
      size_t length = 0;
      TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
          source, index + 1U, record, sizeof(record), length));
      TEST_ASSERT_EQUAL_UINT32(40, length);
      BeaconSettings decoded;
      uint32_t sequence = 0;
      TEST_ASSERT_TRUE(PersistentConfigRecord::decodeWspr(
          record, length, decoded, sequence));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(source.band),
                              static_cast<uint8_t>(decoded.band));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(source.frequencyMode),
                              static_cast<uint8_t>(decoded.frequencyMode));
      TEST_ASSERT_EQUAL(source.powerSave, decoded.powerSave);
      TEST_ASSERT_EQUAL_UINT32(source.baseFrequencyHz,
                               decoded.baseFrequencyHz);
    }
  }
}

void test_every_band_and_mode_recovers_after_manager_reboot() {
  for (uint8_t index = 0; index < BandPlan::size(); ++index) {
    const BandPlanEntry *band = BandPlan::at(index);
    for (uint8_t random = 0; random < 2; ++random) {
      FakeStorage storage;
      ConfigurationManager manager(storage);
      manager.begin();
      BeaconSettings candidate = manager.candidateWspr();
      strcpy(candidate.callsign, "IZ2ABC");
      candidate.band = band->id;
      candidate.baseFrequencyHz = band->nominalToneZeroHz;
      candidate.frequencyMode = random ? WsprFrequencyMode::RANDOM
                                       : WsprFrequencyMode::FIXED;
      TEST_ASSERT_TRUE(manager.setWsprCandidate(candidate));
      save(manager);

      ConfigurationManager rebooted(storage);
      rebooted.begin();
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(candidate.band),
                              static_cast<uint8_t>(rebooted.activeWspr().band));
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(candidate.frequencyMode),
          static_cast<uint8_t>(rebooted.activeWspr().frequencyMode));
      TEST_ASSERT_EQUAL_UINT32(candidate.baseFrequencyHz,
                               rebooted.activeWspr().baseFrequencyHz);
    }
  }
}

void test_wspr_v3_layout_preserves_record_and_storage_sizes() {
  TEST_ASSERT_EQUAL_UINT8(1, PersistentConfigRecord::FORMAT_VERSION);
  TEST_ASSERT_EQUAL_UINT8(2,
      PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V2);
  TEST_ASSERT_EQUAL_UINT8(3,
      PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V3);
  TEST_ASSERT_EQUAL_UINT32(22, PersistentConfigRecord::WSPR_PAYLOAD_SIZE);
  TEST_ASSERT_EQUAL_UINT32(40, PersistentConfigRecord::WSPR_RECORD_SIZE);
  TEST_ASSERT_EQUAL_UINT32(80,
      PersistentConfigRecord::WSPR_RECORD_SIZE * 2U);
  TEST_ASSERT_EQUAL_UINT32(196,
      PersistentConfigRecord::WSPR_RECORD_SIZE * 2U +
      PersistentConfigRecord::WIFI_RECORD_SIZE);
  TEST_ASSERT_EQUAL_UINT32(312,
      PersistentConfigRecord::WSPR_RECORD_SIZE * 2U +
      PersistentConfigRecord::WIFI_RECORD_SIZE * 2U);

  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings source = wsprWithCallsign("IW2NNV");
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      source, 1, record, sizeof(record), length));
  TEST_ASSERT_EQUAL_UINT32(PersistentConfigRecord::HEADER_SIZE + 22, length);
  TEST_ASSERT_EQUAL_UINT8(1, record[4]);
  TEST_ASSERT_EQUAL_UINT8(3, record[6]);
  TEST_ASSERT_EQUAL_UINT8(0, record[7]);
  TEST_ASSERT_EQUAL_UINT8(0, record[32]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
                          record[33]);
}

void test_known_v1_fixture_loads_without_writes_as_20m_fixed() {
  FakeStorage storage;
  storage.place(ConfigRecordType::WSPR, 0, WSPR_V1_FIXTURE,
                sizeof(WSPR_V1_FIXTURE));

  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.savedWspr().callsign);
  TEST_ASSERT_EQUAL_INT16(-25, manager.savedWspr().frequencyOffsetHz);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprBandId::BAND_20M),
      static_cast<uint8_t>(manager.savedWspr().band));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
      static_cast<uint8_t>(manager.savedWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
      static_cast<uint8_t>(manager.candidateWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
      static_cast<uint8_t>(manager.activeWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_TRUE(manager.status().wsprMigrationPending);
  TEST_ASSERT_EQUAL_UINT8(1, manager.status().wsprPayloadVersion);
}

void test_v2_loads_power_save_off_and_migrates_only_on_explicit_save() {
  FakeStorage storage;
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings source = wsprWithCallsign("IZ2ABC");
  source.powerSave = true;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      source, 9, record, sizeof(record), length));
  record[6] = PersistentConfigRecord::WSPR_PAYLOAD_VERSION_V2;
  record[33] &= static_cast<uint8_t>(~0x08U);
  refreshRecordCrc(record);
  storage.place(ConfigRecordType::WSPR, 0, record, length);

  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_FALSE(manager.activeWspr().powerSave);
  TEST_ASSERT_TRUE(manager.status().wsprMigrationPending);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(WsprPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWsprOnly()));
  TEST_ASSERT_EQUAL_UINT8(3, manager.status().wsprPayloadVersion);
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[0]);
}

void test_explicit_save_migrates_unchanged_v1_without_writing_wifi() {
  FakeStorage storage;
  storage.place(ConfigRecordType::WSPR, 0, WSPR_V1_FIXTURE,
                sizeof(WSPR_V1_FIXTURE));
  uint8_t wifiRecord[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t wifiLength = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("Saved network", "saved password"), 7, wifiRecord,
      sizeof(wifiRecord), wifiLength));
  storage.place(ConfigRecordType::WIFI, 0, wifiRecord, wifiLength);
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
      static_cast<uint8_t>(manager.processPending(false)));
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
  TEST_ASSERT_EQUAL_UINT32(wifiLength, storage.slots[1][0].length);
  TEST_ASSERT_EQUAL_MEMORY(wifiRecord, storage.slots[1][0].data, wifiLength);
  TEST_ASSERT_EQUAL_UINT8(3, storage.slots[0][1].data[6]);
  TEST_ASSERT_FALSE(manager.status().wsprMigrationPending);
  TEST_ASSERT_EQUAL_UINT8(3, manager.status().wsprPayloadVersion);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::FIXED),
      static_cast<uint8_t>(manager.savedWspr().frequencyMode));
}

void test_wspr_only_save_migrates_v1_without_touching_wifi() {
  FakeStorage storage;
  storage.place(ConfigRecordType::WSPR, 0, WSPR_V1_FIXTURE,
                sizeof(WSPR_V1_FIXTURE));
  uint8_t wifiRecord[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t wifiLength = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("Saved network", "saved password"), 7, wifiRecord,
      sizeof(wifiRecord), wifiLength));
  storage.place(ConfigRecordType::WIFI, 0, wifiRecord, wifiLength);
  ConfigurationManager manager(storage);
  manager.begin();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWsprOnly()));
  TEST_ASSERT_EQUAL_UINT16(1, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
  TEST_ASSERT_EQUAL_MEMORY(wifiRecord, storage.slots[1][0].data, wifiLength);
  TEST_ASSERT_FALSE(manager.status().wsprMigrationPending);
  TEST_ASSERT_EQUAL_UINT8(3, manager.status().wsprPayloadVersion);
}

void test_mixed_v1_v2_records_are_selected_only_by_sequence() {
  FakeStorage storage;
  storage.place(ConfigRecordType::WSPR, 0, WSPR_V1_FIXTURE,
                sizeof(WSPR_V1_FIXTURE));
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings v2 = wsprWithCallsign("IZ2NEW");
  v2.frequencyMode = WsprFrequencyMode::RANDOM;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      v2, 41, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 1, record, length);

  ConfigurationManager olderV2(storage);
  olderV2.begin();
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", olderV2.activeWspr().callsign);
  TEST_ASSERT_EQUAL_UINT8(1, olderV2.status().wsprPayloadVersion);

  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      v2, 43, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 1, record, length);
  ConfigurationManager newerV2(storage);
  newerV2.begin();
  TEST_ASSERT_EQUAL_STRING("IZ2NEW", newerV2.activeWspr().callsign);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
      static_cast<uint8_t>(newerV2.activeWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(3, newerV2.status().wsprPayloadVersion);
}

void test_v3_rejects_unknown_version_reserved_band_crc_and_truncation() {
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings source = wsprWithCallsign("IW2NNV");
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      source, 3, record, sizeof(record), length));
  BeaconSettings decoded;
  uint32_t sequence = 0;

  uint8_t changed[PersistentConfigRecord::WSPR_RECORD_SIZE];
  memcpy(changed, record, length);
  changed[6] = 4;
  refreshRecordCrc(changed);
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWspr(
      changed, length, decoded, sequence));

  memcpy(changed, record, length);
  changed[33] = 0x10U;
  refreshRecordCrc(changed);
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWspr(
      changed, length, decoded, sequence));

  memcpy(changed, record, length);
  changed[length - 1] ^= 0x01U;
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWspr(
      changed, length, decoded, sequence));
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWspr(
      record, length - 1U, decoded, sequence));
}

void test_saved_random_mode_is_restored_after_reboot() {
  FakeStorage storage;
  ConfigurationManager manager(storage, wsprWithCallsign("IW2NNV"));
  manager.begin();
  BeaconSettings random = manager.candidateWspr();
  strcpy(random.callsign, "IW2NNV");
  random.frequencyMode = WsprFrequencyMode::RANDOM;
  TEST_ASSERT_TRUE(manager.setWsprCandidate(random));
  TEST_ASSERT_TRUE(manager.applyWsprSelection(random.band,
                                              random.frequencyMode));
  save(manager);

  ConfigurationManager rebooted(storage);
  rebooted.begin();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
      static_cast<uint8_t>(rebooted.savedWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
      static_cast<uint8_t>(rebooted.candidateWspr().frequencyMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WsprFrequencyMode::RANDOM),
      static_cast<uint8_t>(rebooted.activeWspr().frequencyMode));
}

void test_wifi_record_round_trip_and_incompatible_version() {
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  WifiCredentials source = wifi("Network with spaces", "secret phrase");
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      source, 7, record, sizeof(record), length));
  WifiCredentials decoded;
  uint32_t sequence = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::decodeWifi(
      record, length, decoded, sequence));
  TEST_ASSERT_EQUAL_STRING(source.ssid, decoded.ssid);
  TEST_ASSERT_EQUAL_STRING(source.password, decoded.password);
  record[4] = PersistentConfigRecord::FORMAT_VERSION + 1;
  TEST_ASSERT_FALSE(PersistentConfigRecord::decodeWifi(
      record, length, decoded, sequence));
}

void test_absent_and_corrupt_records_use_defaults() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::DEFAULTS),
                          static_cast<uint8_t>(manager.status().wsprOrigin));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  storage.slots[0][0].present = true;
  storage.slots[0][0].length = 4;
  memset(storage.slots[0][0].data, 0xAA, 4);
  manager.begin();
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.savedWspr().callsign);
  TEST_ASSERT_TRUE(manager.status().wsprRecordRejected);
  TEST_ASSERT_FALSE(manager.status().wifiRecordRejected);
}

void test_fresh_storage_has_no_rejected_records_or_writes() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  const ConfigurationStatus status = manager.status();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::DEFAULTS),
                          static_cast<uint8_t>(status.wsprOrigin));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::DEFAULTS),
                          static_cast<uint8_t>(status.wifiOrigin));
  TEST_ASSERT_FALSE(status.wsprRecordRejected);
  TEST_ASSERT_FALSE(status.wifiRecordRejected);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_two_valid_ab_records_select_latest_without_rejection_or_writes() {
  FakeStorage storage;
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      wsprWithCallsign("IZ2AAA"), 1, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 0, record, length);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      wsprWithCallsign("IZ2BBB"), 2, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 1, record, length);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("Old network", "old secret"), 1, record, sizeof(record), length));
  storage.place(ConfigRecordType::WIFI, 0, record, length);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("New network", "new secret"), 2, record, sizeof(record), length));
  storage.place(ConfigRecordType::WIFI, 1, record, length);

  ConfigurationManager manager(storage);
  manager.begin();
  const ConfigurationStatus status = manager.status();
  TEST_ASSERT_EQUAL_STRING("IZ2BBB", manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("New network", manager.activeWifi().ssid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
                          static_cast<uint8_t>(status.wsprOrigin));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
                          static_cast<uint8_t>(status.wifiOrigin));
  TEST_ASSERT_FALSE(status.wsprRecordRejected);
  TEST_ASSERT_FALSE(status.wifiRecordRejected);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_zeroed_slot_and_valid_slot_recover_both_saved_domains_without_writes() {
  FakeStorage storage;
  uint8_t invalid[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
  storage.place(ConfigRecordType::WSPR, 0, invalid,
                PersistentConfigRecord::WSPR_RECORD_SIZE);
  storage.place(ConfigRecordType::WIFI, 0, invalid,
                PersistentConfigRecord::WIFI_RECORD_SIZE);
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      wsprWithCallsign("IZ2ABC"), 1, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 1, record, length);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWifi(
      wifi("Saved network", "saved secret"), 1, record, sizeof(record),
      length));
  storage.place(ConfigRecordType::WIFI, 1, record, length);

  ConfigurationManager manager(storage);
  manager.begin();
  const ConfigurationStatus status = manager.status();
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("Saved network", manager.activeWifi().ssid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
                          static_cast<uint8_t>(status.wsprOrigin));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
                          static_cast<uint8_t>(status.wifiOrigin));
  TEST_ASSERT_TRUE(status.wsprRecordRejected);
  TEST_ASSERT_TRUE(status.wifiRecordRejected);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_both_invalid_slots_use_defaults_without_writes() {
  FakeStorage storage;
  uint8_t invalid[PersistentConfigRecord::MAX_RECORD_SIZE] = {};
  for (uint8_t slot = 0; slot < 2; ++slot) {
    storage.place(ConfigRecordType::WSPR, slot, invalid,
                  PersistentConfigRecord::WSPR_RECORD_SIZE);
    storage.place(ConfigRecordType::WIFI, slot, invalid,
                  PersistentConfigRecord::WIFI_RECORD_SIZE);
  }
  ConfigurationManager manager(storage);
  manager.begin();
  const ConfigurationStatus status = manager.status();
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::DEFAULTS),
                          static_cast<uint8_t>(status.wsprOrigin));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::DEFAULTS),
                          static_cast<uint8_t>(status.wifiOrigin));
  TEST_ASSERT_TRUE(status.wsprRecordRejected);
  TEST_ASSERT_TRUE(status.wifiRecordRejected);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_storage_read_failure_uses_defaults_without_writes() {
  FakeStorage storage;
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings saved = wsprWithCallsign("IZ2ABC");
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      saved, 3, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 0, record, length);
  storage.failReads = true;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_latest_compatible_sequence_is_selected_with_wrap() {
  FakeStorage storage;
  uint8_t record[PersistentConfigRecord::MAX_RECORD_SIZE];
  size_t length = 0;
  BeaconSettings oldValue = wsprWithCallsign("IZ2AAA");
  BeaconSettings newValue = wsprWithCallsign("IZ2BBB");
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      oldValue, UINT32_MAX, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 0, record, length);
  TEST_ASSERT_TRUE(PersistentConfigRecord::encodeWspr(
      newValue, 0, record, sizeof(record), length));
  storage.place(ConfigRecordType::WSPR, 1, record, length);
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_EQUAL_STRING("IZ2BBB", manager.activeWspr().callsign);
  TEST_ASSERT_TRUE(PersistentConfigRecord::sequenceNewer(0, UINT32_MAX));
}

void test_interrupted_write_preserves_previous_active_and_saved_record() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wsprWithCallsign("IZ2AAA")));
  save(manager);
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wsprWithCallsign("IZ2BBB")));
  storage.interruptedBytes = 12;
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::FAILED),
                          static_cast<uint8_t>(manager.processPending(false)));
  TEST_ASSERT_EQUAL_STRING("IZ2AAA", manager.activeWspr().callsign);
  ConfigurationManager rebooted(storage);
  rebooted.begin();
  TEST_ASSERT_EQUAL_STRING("IZ2AAA", rebooted.activeWspr().callsign);
  TEST_ASSERT_TRUE(rebooted.status().wsprRecordRejected);
}

void test_wifi_and_wspr_records_are_independent() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wsprWithCallsign("IZ2ABC")));
  TEST_ASSERT_TRUE(manager.setWifiCandidate(wifi("My network", "secret phrase")));
  storage.failType = ConfigRecordType::WIFI;
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::PARTIAL),
                          static_cast<uint8_t>(manager.processPending(false)));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.savedWspr().callsign);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
}

void test_unchanged_records_are_not_written() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
#if defined(WSPR_PROFILE_DEVELOPMENT)
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::UNCHANGED),
                          static_cast<uint8_t>(manager.processPending(false)));
#else
  TEST_ASSERT_FALSE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::INVALID),
                          static_cast<uint8_t>(manager.processPending(false)));
#endif
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
}

void test_invalid_candidates_are_rejected() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  BeaconSettings invalid;
  strcpy(invalid.callsign, "BAD");
  TEST_ASSERT_FALSE(manager.setWsprCandidate(invalid));
  WifiCredentials incomplete;
  TEST_ASSERT_TRUE(incomplete.setSsid("network"));
  TEST_ASSERT_FALSE(manager.setWifiCandidate(incomplete));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.candidateWspr().callsign);
}

void test_default_saved_candidate_and_active_transitions() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wsprWithCallsign("IZ2ABC")));
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.savedWspr().callsign);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  save(manager);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.savedWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  manager.loadDefaultsAsCandidates();
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.candidateWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  manager.discardCandidates();
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.candidateWspr().callsign);
}

void test_tx_defers_writes_and_active_changes() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.setWsprCandidate(wsprWithCallsign("IZ2ABC")));
  TEST_ASSERT_TRUE(manager.setWifiCandidate(wifi("My network", "secret phrase")));
  TEST_ASSERT_TRUE(manager.requestSave());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::DEFERRED),
                          static_cast<uint8_t>(manager.processPending(true)));
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[0]);
  TEST_ASSERT_EQUAL_UINT16(0, storage.writes[1]);
  TEST_ASSERT_EQUAL_STRING(WSPR_DEFAULT_CALLSIGN,
                           manager.activeWspr().callsign);
  TEST_ASSERT_FALSE(manager.activeWifi().hasSsid());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigSaveResult::SAVED),
                          static_cast<uint8_t>(manager.processPending(false)));
  TEST_ASSERT_EQUAL_STRING("IZ2ABC", manager.activeWspr().callsign);
  TEST_ASSERT_EQUAL_STRING("My network", manager.activeWifi().ssid);
}

void test_status_does_not_expose_password() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  TEST_ASSERT_TRUE(manager.setWifiCandidate(wifi("My network", "secret phrase")));
  const ConfigurationStatus status = manager.status();
  TEST_ASSERT_FALSE(containsBytes(&status, sizeof(status), "secret phrase"));
  TEST_ASSERT_TRUE(status.wifiDirty);
}

void test_saved_wifi_is_available_for_automatic_startup_connection() {
  FakeStorage storage;
  ConfigurationManager manager(storage);
  manager.begin();
  const WifiCredentials credentials =
      wifi("Saved startup network", "startup secret");
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiPersistenceResult::SAVED),
      static_cast<uint8_t>(manager.saveWifiOnly(credentials)));
  ConfigurationManager rebooted(storage);
  rebooted.begin();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigOrigin::SAVED),
      static_cast<uint8_t>(rebooted.status().wifiOrigin));
  TEST_ASSERT_TRUE(rebooted.activeWifi().complete());
  TEST_ASSERT_EQUAL_STRING("Saved startup network",
                           rebooted.activeWifi().ssid);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_v3_round_trip_preserves_all_fields_and_crc);
  RUN_TEST(test_v3_round_trip_preserves_every_band_mode_and_power_save);
  RUN_TEST(test_every_band_and_mode_recovers_after_manager_reboot);
  RUN_TEST(test_wspr_v3_layout_preserves_record_and_storage_sizes);
  RUN_TEST(test_known_v1_fixture_loads_without_writes_as_20m_fixed);
  RUN_TEST(test_v2_loads_power_save_off_and_migrates_only_on_explicit_save);
  RUN_TEST(test_explicit_save_migrates_unchanged_v1_without_writing_wifi);
  RUN_TEST(test_wspr_only_save_migrates_v1_without_touching_wifi);
  RUN_TEST(test_mixed_v1_v2_records_are_selected_only_by_sequence);
  RUN_TEST(test_v3_rejects_unknown_version_reserved_band_crc_and_truncation);
  RUN_TEST(test_saved_random_mode_is_restored_after_reboot);
  RUN_TEST(test_wifi_record_round_trip_and_incompatible_version);
  RUN_TEST(test_open_wifi_record_uses_v2_and_old_protected_record_still_loads);
  RUN_TEST(test_absent_and_corrupt_records_use_defaults);
  RUN_TEST(test_fresh_storage_has_no_rejected_records_or_writes);
  RUN_TEST(test_two_valid_ab_records_select_latest_without_rejection_or_writes);
  RUN_TEST(test_zeroed_slot_and_valid_slot_recover_both_saved_domains_without_writes);
  RUN_TEST(test_both_invalid_slots_use_defaults_without_writes);
  RUN_TEST(test_storage_read_failure_uses_defaults_without_writes);
  RUN_TEST(test_latest_compatible_sequence_is_selected_with_wrap);
  RUN_TEST(test_interrupted_write_preserves_previous_active_and_saved_record);
  RUN_TEST(test_wifi_and_wspr_records_are_independent);
  RUN_TEST(test_unchanged_records_are_not_written);
  RUN_TEST(test_invalid_candidates_are_rejected);
  RUN_TEST(test_default_saved_candidate_and_active_transitions);
  RUN_TEST(test_tx_defers_writes_and_active_changes);
  RUN_TEST(test_status_does_not_expose_password);
  RUN_TEST(test_saved_wifi_is_available_for_automatic_startup_connection);
  return UNITY_END();
}
