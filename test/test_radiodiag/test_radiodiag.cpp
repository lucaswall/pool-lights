#include <unity.h>

#include "radiodiag.h"

void setUp(void) {}
void tearDown(void) {}

static void readback_matches(void) {
  TEST_ASSERT_EQUAL(RADIO_OK, diagnose(0x4c, 0x4c));
}

// A dump of all zeroes means MISO never went high: the module is unpowered, GND is not
// shared, or MISO is not connected.
static void all_zero_is_dead_miso(void) {
  TEST_ASSERT_EQUAL(RADIO_MISO_LOW, diagnose(0x4c, 0x00));
}

// All ones means nothing is driving MISO — no chip on the other end of the bus.
static void all_ones_is_floating(void) {
  TEST_ASSERT_EQUAL(RADIO_MISO_FLOATING, diagnose(0x4c, 0xff));
}

// Anything else came back corrupted: the bus works but not reliably, which on dupont wire
// is usually the SPI clock being too fast.
static void other_mismatch_is_unstable(void) {
  TEST_ASSERT_EQUAL(RADIO_UNSTABLE, diagnose(0x4c, 0x4d));
}

// 0x00 and 0xff are only faults when they are not what we asked for.
static void expected_extremes_are_not_faults(void) {
  TEST_ASSERT_EQUAL(RADIO_OK, diagnose(0x00, 0x00));
  TEST_ASSERT_EQUAL(RADIO_OK, diagnose(0xff, 0xff));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(readback_matches);
  RUN_TEST(all_zero_is_dead_miso);
  RUN_TEST(all_ones_is_floating);
  RUN_TEST(other_mismatch_is_unstable);
  RUN_TEST(expected_extremes_are_not_faults);
  return UNITY_END();
}
