#include <unity.h>

#include "packet_queue.h"

void setUp(void) {}
void tearDown(void) {}

// Byte 0 is the key, which v2Build pins to 0x00 on every real packet, so a test that only
// reads out[0] would pass even if the queue copied a single byte.
static void fill(uint8_t *packet, uint8_t marker) {
  packet[0] = 0x00;
  for (uint8_t i = 1; i < V2_PACKET_LEN; i++) {
    packet[i] = (uint8_t)(marker + i);
  }
}

static void empty_queue_yields_nothing(void) {
  PacketQueue queue;
  uint8_t out[V2_PACKET_LEN];
  TEST_ASSERT_FALSE(queue.pop(out));
}

// The bug this exists to prevent: one Home Assistant message can carry colour, brightness
// and power, which become three commands. A single-slot buffer sends only the last.
static void holds_several_commands_in_order(void) {
  PacketQueue queue;
  uint8_t packet[V2_PACKET_LEN];
  for (uint8_t i = 1; i <= 3; i++) {
    fill(packet, i);
    queue.push(packet);
  }

  uint8_t out[V2_PACKET_LEN];
  for (uint8_t i = 1; i <= 3; i++) {
    TEST_ASSERT_TRUE(queue.pop(out));
    uint8_t want[V2_PACKET_LEN];
    fill(want, i);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, V2_PACKET_LEN);
  }
  TEST_ASSERT_FALSE(queue.pop(out));
}

// Overflow drops the oldest, because the newest command is the one that reflects what
// somebody just asked for.
static void overflow_drops_the_oldest(void) {
  PacketQueue queue;
  uint8_t packet[V2_PACKET_LEN];
  for (uint8_t i = 1; i <= PACKET_QUEUE_LEN + 2; i++) {
    fill(packet, i);
    queue.push(packet);
  }

  uint8_t out[V2_PACKET_LEN], want[V2_PACKET_LEN];
  TEST_ASSERT_TRUE(queue.pop(out));
  fill(want, 3);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, out, V2_PACKET_LEN);   // 1 and 2 were pushed out

  // Counting the survivors catches an overflow branch that evicts more than one.
  uint8_t drained = 1;
  while (queue.pop(out)) {
    drained++;
  }
  TEST_ASSERT_EQUAL_UINT8(PACKET_QUEUE_LEN, drained);
}

// Indices wrap, so the queue has to keep working long after the buffer has been lapped.
static void survives_wrapping(void) {
  PacketQueue queue;
  uint8_t packet[V2_PACKET_LEN];
  uint8_t out[V2_PACKET_LEN];

  for (uint8_t round = 0; round < 40; round++) {
    fill(packet, round);
    queue.push(packet);
    TEST_ASSERT_TRUE(queue.pop(out));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(packet, out, V2_PACKET_LEN);
  }
  TEST_ASSERT_FALSE(queue.pop(out));
}

// A dropped command should not be silent: it is the difference between "the light
// ignored me" and "we never sent it".
static void push_reports_when_it_evicts(void) {
  PacketQueue queue;
  uint8_t packet[V2_PACKET_LEN];
  for (uint8_t i = 0; i < PACKET_QUEUE_LEN; i++) {
    fill(packet, i);
    TEST_ASSERT_TRUE(queue.push(packet));
  }
  fill(packet, 99);
  TEST_ASSERT_FALSE(queue.push(packet));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(empty_queue_yields_nothing);
  RUN_TEST(holds_several_commands_in_order);
  RUN_TEST(overflow_drops_the_oldest);
  RUN_TEST(survives_wrapping);
  RUN_TEST(push_reports_when_it_evicts);
  return UNITY_END();
}
