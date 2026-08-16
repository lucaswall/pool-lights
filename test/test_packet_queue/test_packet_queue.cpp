#include <unity.h>

#include "packet_queue.h"

void setUp(void) {}
void tearDown(void) {}

static void fill(uint8_t *packet, uint8_t marker) {
  for (uint8_t i = 0; i < V2_PACKET_LEN; i++) {
    packet[i] = marker;
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
    TEST_ASSERT_EQUAL_UINT8(i, out[0]);
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

  uint8_t out[V2_PACKET_LEN];
  TEST_ASSERT_TRUE(queue.pop(out));
  TEST_ASSERT_EQUAL_UINT8(3, out[0]);   // 1 and 2 were pushed out

  uint8_t last = 0;
  while (queue.pop(out)) {
    last = out[0];
  }
  TEST_ASSERT_EQUAL_UINT8(PACKET_QUEUE_LEN + 2, last);
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
    TEST_ASSERT_EQUAL_UINT8(round, out[0]);
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
