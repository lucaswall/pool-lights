#include <string.h>
#include <unity.h>

#include "netid.h"

void setUp(void) {}
void tearDown(void) {}

static void names_from_chip_id(void) {
  char buf[NETID_LEN];
  deviceName(0x0abc12, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("pool-lights-0abc12", buf);
}

// ESP.getChipId() is already 24-bit, but a caller passing a full 32-bit word must not
// produce a longer name than the buffer OTA and DHCP were sized for.
static void ignores_high_byte(void) {
  char buf[NETID_LEN];
  deviceName(0xff0abc12, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("pool-lights-0abc12", buf);
}

static void pads_short_ids(void) {
  char buf[NETID_LEN];
  deviceName(0x1, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("pool-lights-000001", buf);
}

static void fits_exactly(void) {
  char buf[NETID_LEN];
  deviceName(0xffffff, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_UINT(NETID_LEN - 1, strlen(buf));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(names_from_chip_id);
  RUN_TEST(ignores_high_byte);
  RUN_TEST(pads_short_ids);
  RUN_TEST(fits_exactly);
  return UNITY_END();
}
