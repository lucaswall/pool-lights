#include <string.h>
#include <unity.h>

#include "milight_frame.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t PAYLOAD[9] = {0x00, 0x25, 0xab, 0xcd, 0x01, 0x01, 0x07, 0x03, 0x2f};

// Computed from the frame definition rather than from this implementation: length byte,
// payload, CRC-16/0x8408 over both, then every byte bit-reversed.
static const uint8_t FRAME[12] = {0x90, 0x00, 0xa4, 0xd5, 0xb3, 0x80,
                                  0x80, 0xe0, 0xc0, 0xf4, 0x12, 0xb5};

static void builds_the_known_frame(void) {
  uint8_t out[32];
  const uint8_t len = milightFrame(PAYLOAD, sizeof(PAYLOAD), out);
  TEST_ASSERT_EQUAL_UINT8(sizeof(FRAME), len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(FRAME, out, sizeof(FRAME));
}

static void parses_the_known_frame(void) {
  uint8_t payload[16];
  const uint8_t len = milightParseFrame(FRAME, sizeof(FRAME), payload, sizeof(payload));
  TEST_ASSERT_EQUAL_UINT8(sizeof(PAYLOAD), len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(PAYLOAD, payload, sizeof(PAYLOAD));
}

static void round_trips(void) {
  uint8_t frame[32], payload[16];
  const uint8_t frameLen = milightFrame(PAYLOAD, sizeof(PAYLOAD), frame);
  TEST_ASSERT_EQUAL_UINT8(sizeof(PAYLOAD),
                          milightParseFrame(frame, frameLen, payload, sizeof(payload)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(PAYLOAD, payload, sizeof(PAYLOAD));
}

// The CRC is the only thing separating our traffic from anything else that matches the
// NRF24 address, so a corrupted frame must be rejected rather than decoded.
static void rejects_a_corrupted_frame(void) {
  uint8_t frame[32], payload[16];
  memcpy(frame, FRAME, sizeof(FRAME));
  frame[4] ^= 0x01;
  TEST_ASSERT_EQUAL_UINT8(0, milightParseFrame(frame, sizeof(FRAME), payload,
                                               sizeof(payload)));
}

// A frame whose CRC happens to pass but whose length byte disagrees is not ours.
static void rejects_a_disagreeing_length_byte(void) {
  uint8_t frame[32], payload[16];
  const uint8_t frameLen = milightFrame(PAYLOAD, sizeof(PAYLOAD), frame);
  TEST_ASSERT_EQUAL_UINT8(0, milightParseFrame(frame, (uint8_t)(frameLen - 1), payload,
                                               sizeof(payload)));
}

static void refuses_to_overrun_the_caller(void) {
  uint8_t payload[4];
  TEST_ASSERT_EQUAL_UINT8(0, milightParseFrame(FRAME, sizeof(FRAME), payload,
                                               sizeof(payload)));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(builds_the_known_frame);
  RUN_TEST(parses_the_known_frame);
  RUN_TEST(round_trips);
  RUN_TEST(rejects_a_corrupted_frame);
  RUN_TEST(rejects_a_disagreeing_length_byte);
  RUN_TEST(refuses_to_overrun_the_caller);
  return UNITY_END();
}
