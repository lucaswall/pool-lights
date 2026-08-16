#include <string.h>
#include <unity.h>

#include "log_buffer.h"

void setUp(void) {}
void tearDown(void) {}

static void starts_empty(void) {
  LogBuffer log;
  TEST_ASSERT_EQUAL_UINT8(0, log.count());
}

// Oldest first, so a browser reading /log sees the boot banner at the top.
static void keeps_insertion_order(void) {
  LogBuffer log;
  log.push("first");
  log.push("second");
  log.push("third");

  TEST_ASSERT_EQUAL_UINT8(3, log.count());
  TEST_ASSERT_EQUAL_STRING("first", log.line(0));
  TEST_ASSERT_EQUAL_STRING("second", log.line(1));
  TEST_ASSERT_EQUAL_STRING("third", log.line(2));
}

// The whole point of a ring: it must run for weeks without growing.
static void drops_the_oldest_when_full(void) {
  LogBuffer log;
  char line[16];
  for (uint16_t i = 0; i < LOG_LINES + 5; i++) {
    snprintf(line, sizeof(line), "line%u", i);
    log.push(line);
  }

  TEST_ASSERT_EQUAL_UINT8(LOG_LINES, log.count());
  TEST_ASSERT_EQUAL_STRING("line5", log.line(0));

  snprintf(line, sizeof(line), "line%u", LOG_LINES + 4);
  TEST_ASSERT_EQUAL_STRING(line, log.line(LOG_LINES - 1));
}

// A long line must be cut, not allowed to run off the end of its slot.
static void truncates_long_lines(void) {
  LogBuffer log;
  char huge[LOG_LINE_LEN * 2];
  memset(huge, 'x', sizeof(huge) - 1);
  huge[sizeof(huge) - 1] = '\0';
  log.push(huge);

  TEST_ASSERT_EQUAL_UINT(LOG_LINE_LEN - 1, strlen(log.line(0)));
}

// Reading past the end is a caller bug, but it must not read out of bounds.
static void out_of_range_is_empty(void) {
  LogBuffer log;
  log.push("only");
  TEST_ASSERT_EQUAL_STRING("", log.line(1));
  TEST_ASSERT_EQUAL_STRING("", log.line(200));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(starts_empty);
  RUN_TEST(keeps_insertion_order);
  RUN_TEST(drops_the_oldest_when_full);
  RUN_TEST(truncates_long_lines);
  RUN_TEST(out_of_range_is_empty);
  return UNITY_END();
}
