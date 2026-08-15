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


// Known-answer vectors, computed independently from the algorithm rather than from this
// implementation. Device 0xABCD is a documentation value: real captured packets carry the
// RF credential of a real light and must never enter the tree (RULE 0).
static void builds_known_packets(void) {
  struct Case { const char *name; uint8_t cmd, arg, seq; uint8_t expected[V2_PACKET_LEN]; };
  const Case cases[] = {
    {"on",     0x01, 0x01, 0, {0x00,0xd8,0x48,0xe8,0x66,0xd1,0xba,0x66,0x4f}},
    {"off",    0x01, 0x0a, 1, {0x00,0xd8,0x48,0xe8,0x66,0xd6,0xbb,0x66,0x35}},
    {"red",    0x02, 0x00, 2, {0x00,0xd8,0x48,0xe8,0x63,0xd0,0xb8,0x66,0x4d}},
    {"bright", 0x05, 0x64, 3, {0x00,0xd8,0x48,0xe8,0x62,0xec,0xb9,0x66,0xd5}},
    {"night",  0x81, 0x0a, 4, {0x00,0xd8,0x48,0xe8,0xe6,0xd6,0xb6,0x66,0xb4}},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    uint8_t packet[V2_PACKET_LEN];
    v2Build(packet, 0xabcd, 1, cases[i].cmd, cases[i].arg, cases[i].seq);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(cases[i].expected, packet, V2_PACKET_LEN,
                                         cases[i].name);
  }
}

static void built_packets_decode_back(void) {
  uint8_t packet[V2_PACKET_LEN];
  v2Build(packet, 0xabcd, 1, V2_CMD_ON_OFF, v2OnArg(1), 7);
  v2Decode(packet);
  TEST_ASSERT_EQUAL_HEX16(0xabcd, v2DeviceId(packet));
  TEST_ASSERT_TRUE(v2IsFut089(packet));
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_ON_OFF, packet[V2_COMMAND]);
  TEST_ASSERT_EQUAL_HEX8(1, packet[V2_GROUP]);
}

// Bit 7 is the held flag, so a decoder comparing the raw byte misses every long press.
static void held_flag_is_separable(void) {
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_ON_OFF, v2BaseCommand(V2_CMD_ON_OFF | V2_HELD));
  TEST_ASSERT_TRUE(v2IsHeld(V2_CMD_ON_OFF | V2_HELD));
  TEST_ASSERT_FALSE(v2IsHeld(V2_CMD_ON_OFF));
}

static void group_args(void) {
  TEST_ASSERT_EQUAL_HEX8(0x01, v2OnArg(1));
  TEST_ASSERT_EQUAL_HEX8(0x0a, v2OffArg(1));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(xor_key_known_vectors);
  RUN_TEST(encode_decode_round_trip);
  RUN_TEST(round_trip_holds_for_every_key);
  RUN_TEST(reads_device_id_and_fields);
  RUN_TEST(rejects_other_protocols);
  RUN_TEST(builds_known_packets);
  RUN_TEST(built_packets_decode_back);
  RUN_TEST(held_flag_is_separable);
  RUN_TEST(group_args);
  return UNITY_END();
}
