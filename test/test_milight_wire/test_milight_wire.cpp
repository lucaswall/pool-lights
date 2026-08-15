#include <unity.h>

#include "milight_wire.h"

void setUp(void) {}
void tearDown(void) {}

static void reverses_bits(void) {
  TEST_ASSERT_EQUAL_HEX8(0x00, reverseBits(0x00));
  TEST_ASSERT_EQUAL_HEX8(0xff, reverseBits(0xff));
  TEST_ASSERT_EQUAL_HEX8(0x80, reverseBits(0x01));
  TEST_ASSERT_EQUAL_HEX8(0x01, reverseBits(0x80));
  TEST_ASSERT_EQUAL_HEX8(0x55, reverseBits(0xaa));
  TEST_ASSERT_EQUAL_HEX8(0xaa, reverseBits(0x55));
}

static void reversing_twice_is_identity(void) {
  for (uint16_t b = 0; b <= 0xff; b++) {
    TEST_ASSERT_EQUAL_HEX8(b, reverseBits(reverseBits((uint8_t)b)));
  }
}

// Vectors computed independently from the CRC-16 definition (poly 0x8408, init 0),
// not copied from the implementation being tested.
static void crc_known_vectors(void) {
  const uint8_t empty[1] = {0};
  TEST_ASSERT_EQUAL_HEX16(0x0000, milightCrc(empty, 0));

  const uint8_t zero[1] = {0x00};
  TEST_ASSERT_EQUAL_HEX16(0x0000, milightCrc(zero, 1));

  const uint8_t packet[4] = {0x09, 0x25, 0xab, 0xcd};
  TEST_ASSERT_EQUAL_HEX16(0x955b, milightCrc(packet, 4));

  const uint8_t ones[4] = {0xff, 0xff, 0xff, 0xff};
  TEST_ASSERT_EQUAL_HEX16(0xf399, milightCrc(ones, 4));
}

// The rgb+cct / FUT089 radio config: syncwords 0x7236 and 0x1809, preamble 0xAA,
// trailer 0x05. Getting this wrong means the NRF24 matches nothing and the sniff looks
// like a dead radio, so it is worth pinning.
static void fut089_syncword(void) {
  const uint8_t expected[MILIGHT_SYNCWORD_LEN] = {0x8a, 0x01, 0xe9, 0xc4, 0x56};
  uint8_t actual[MILIGHT_SYNCWORD_LEN];
  milightSyncword(0x7236, 0x1809, 0xaa, 0x05, actual);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, MILIGHT_SYNCWORD_LEN);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(reverses_bits);
  RUN_TEST(reversing_twice_is_identity);
  RUN_TEST(crc_known_vectors);
  RUN_TEST(fut089_syncword);
  return UNITY_END();
}
