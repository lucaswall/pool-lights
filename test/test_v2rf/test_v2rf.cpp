#include <string.h>
#include <unity.h>

#include "v2rf.h"

void setUp(void) {}
void tearDown(void) {}

static void xor_key_known_vectors(void) {
  TEST_ASSERT_EQUAL_HEX8(0xb6, v2XorKey(0x00));
  TEST_ASSERT_EQUAL_HEX8(0xb7, v2XorKey(0x01));
  TEST_ASSERT_EQUAL_HEX8(0x9a, v2XorKey(0x54));
  TEST_ASSERT_EQUAL_HEX8(0x4d, v2XorKey(0xab));
  TEST_ASSERT_EQUAL_HEX8(0xb1, v2XorKey(0xff));
}

// The property that matters on air: whatever we encode, a receiver decodes back. Upstream
// has no test for this at all, and it is the whole protocol.
static void encode_decode_round_trip(void) {
  uint8_t original[V2_PACKET_LEN] = {0x00, 0x25, 0xab, 0xcd, 0x01, 0x04, 0x07, 0x03, 0x00};
  uint8_t packet[V2_PACKET_LEN];
  memcpy(packet, original, V2_PACKET_LEN);

  v2Encode(packet);
  TEST_ASSERT_FALSE(memcmp(packet, original, V2_PACKET_LEN) == 0);  // it really did encode

  v2Decode(packet);
  // The checksum byte is computed by the encoder, so it is not expected to survive.
  TEST_ASSERT_EQUAL_HEX8_ARRAY(original, packet, V2_CHECKSUM);
}

// The key byte varies per packet and drives the whole obfuscation, so the round trip has
// to hold for every one of them, not just the 0x00 the encoder happens to use.
static void round_trip_holds_for_every_key(void) {
  for (uint16_t key = 0; key <= 0xff; key++) {
    uint8_t original[V2_PACKET_LEN] = {(uint8_t)key, 0x25, 0x12, 0x34, 0x05, 0x64, 0x09, 0x02, 0x00};
    uint8_t packet[V2_PACKET_LEN];
    memcpy(packet, original, V2_PACKET_LEN);

    v2Encode(packet);
    v2Decode(packet);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original, packet, V2_CHECKSUM);
  }
}

static void reads_device_id_and_fields(void) {
  const uint8_t decoded[V2_PACKET_LEN] = {0x00, 0x25, 0xab, 0xcd, 0x01, 0x04, 0x07, 0x03, 0x00};
  TEST_ASSERT_EQUAL_HEX16(0xabcd, v2DeviceId(decoded));
  TEST_ASSERT_EQUAL_HEX8(0x25, decoded[V2_PROTOCOL]);
  TEST_ASSERT_EQUAL_HEX8(0x03, decoded[V2_GROUP]);
  TEST_ASSERT_TRUE(v2IsFut089(decoded));
}

static void rejects_other_protocols(void) {
  const uint8_t decoded[V2_PACKET_LEN] = {0x00, 0x20, 0xab, 0xcd, 0x01, 0x04, 0x07, 0x03, 0x00};
  TEST_ASSERT_FALSE(v2IsFut089(decoded));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(xor_key_known_vectors);
  RUN_TEST(encode_decode_round_trip);
  RUN_TEST(round_trip_holds_for_every_key);
  RUN_TEST(reads_device_id_and_fields);
  RUN_TEST(rejects_other_protocols);
  return UNITY_END();
}
