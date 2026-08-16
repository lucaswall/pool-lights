#include <string.h>
#include <unity.h>

#include "encoder.h"
#include "packet.h"

void setUp(void) {}
void tearDown(void) {}

static void decodes_what_the_encoder_built(void) {
  Encoder encoder(0xabcd, 3);
  uint8_t raw[V2_PACKET_LEN];
  encoder.encode(V2_CMD_BRIGHTNESS, 42, raw);

  Packet packet;
  TEST_ASSERT_TRUE(decodePacket(raw, &packet));
  TEST_ASSERT_EQUAL_HEX16(0xabcd, packet.deviceId);
  TEST_ASSERT_EQUAL_UINT8(3, packet.group);
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_BRIGHTNESS, packet.command);
  TEST_ASSERT_EQUAL_UINT8(42, packet.argument);
  TEST_ASSERT_FALSE(packet.held);
}

// The held bit belongs in its own field: leaving it on the command means every comparison
// downstream has to remember to mask, and upstream's decoder gets this wrong.
static void splits_the_held_bit_out(void) {
  Encoder encoder(0x0001, 1);
  uint8_t raw[V2_PACKET_LEN];
  encoder.encode(V2_CMD_ON_OFF | V2_HELD, v2OffArg(1), raw);

  Packet packet;
  TEST_ASSERT_TRUE(decodePacket(raw, &packet));
  TEST_ASSERT_TRUE(packet.held);
  TEST_ASSERT_EQUAL_HEX8(V2_CMD_ON_OFF, packet.command);
}

static void rejects_other_protocols(void) {
  uint8_t raw[V2_PACKET_LEN] = {0x00, 0x20, 0xab, 0xcd, 0x01, 0x01, 0x00, 0x01, 0x00};
  v2Encode(raw);
  Packet packet;
  TEST_ASSERT_FALSE(decodePacket(raw, &packet));
}

// Sequence numbers let a receiver tell a repeat from a fresh press.
static void encoder_advances_the_sequence(void) {
  Encoder encoder(0x1234, 1);
  uint8_t a[V2_PACKET_LEN], b[V2_PACKET_LEN];
  encoder.encode(V2_CMD_ON_OFF, 1, a);
  encoder.encode(V2_CMD_ON_OFF, 1, b);

  Packet first, second;
  decodePacket(a, &first);
  decodePacket(b, &second);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(first.sequence + 1), second.sequence);
}

// Two boards, or the same board across a reboot, must not open with an identical packet:
// the receiver uses the sequence to tell a repeat from a fresh press, and we share the
// physical remote's device id.
static void the_sequence_can_be_seeded(void) {
  Encoder a(0xabcd, 1, 0x00);
  Encoder b(0xabcd, 1, 0x5c);
  uint8_t first[V2_PACKET_LEN], second[V2_PACKET_LEN];
  a.encode(V2_CMD_ON_OFF, v2OnArg(1), first);
  b.encode(V2_CMD_ON_OFF, v2OnArg(1), second);

  TEST_ASSERT_FALSE(memcmp(first, second, V2_PACKET_LEN) == 0);

  Packet pa, pb;
  decodePacket(first, &pa);
  decodePacket(second, &pb);
  TEST_ASSERT_EQUAL_UINT8(0x00, pa.sequence);
  TEST_ASSERT_EQUAL_UINT8(0x5c, pb.sequence);
  TEST_ASSERT_EQUAL_HEX8(pa.command, pb.command);   // same intent, different packet
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(decodes_what_the_encoder_built);
  RUN_TEST(splits_the_held_bit_out);
  RUN_TEST(rejects_other_protocols);
  RUN_TEST(encoder_advances_the_sequence);
  RUN_TEST(the_sequence_can_be_seeded);
  return UNITY_END();
}
