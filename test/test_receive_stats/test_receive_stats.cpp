#include <unity.h>

#include "receive_stats.h"

void setUp(void) {}
void tearDown(void) {}

static void starts_with_nothing_to_report(void) {
  ReceiveStats stats;
  TEST_ASSERT_FALSE(stats.hasData());
  TEST_ASSERT_EQUAL_UINT8(0, stats.missed());
}

// The first press establishes where the remote's counter is; there is no transition yet.
static void the_first_press_is_only_a_reference(void) {
  ReceiveStats stats;
  stats.observe(40);
  TEST_ASSERT_FALSE(stats.hasData());
}

static void consecutive_presses_are_clean(void) {
  ReceiveStats stats;
  for (uint8_t s = 40; s < 50; s++) {
    stats.observe(s);
  }
  TEST_ASSERT_EQUAL_UINT8(9, stats.heard());
  TEST_ASSERT_EQUAL_UINT8(0, stats.missed());
  TEST_ASSERT_EQUAL_UINT8(100, stats.capturePercent());
}

// A gap of a few is the case worth counting: presses that happened and we did not hear.
static void a_small_gap_counts_as_missed(void) {
  ReceiveStats stats;
  stats.observe(10);
  stats.observe(13);   // 11 and 12 never arrived
  TEST_ASSERT_EQUAL_UINT8(1, stats.heard());
  TEST_ASSERT_EQUAL_UINT8(2, stats.missed());
}

// A large gap is someone using the remote in a session we were not part of — while we
// were rebooting, or hours earlier. Counting it would make the figure meaningless.
static void a_large_gap_is_a_new_session_not_a_fault(void) {
  ReceiveStats stats;
  stats.observe(10);
  stats.observe(90);
  TEST_ASSERT_FALSE(stats.hasData());

  stats.observe(91);
  TEST_ASSERT_EQUAL_UINT8(1, stats.heard());
  TEST_ASSERT_EQUAL_UINT8(0, stats.missed());
}

// The sequence byte wraps, and a wrap is not eighty missed presses.
static void the_counter_wraps(void) {
  ReceiveStats stats;
  stats.observe(254);
  stats.observe(255);
  stats.observe(0);
  stats.observe(1);
  TEST_ASSERT_EQUAL_UINT8(3, stats.heard());
  TEST_ASSERT_EQUAL_UINT8(0, stats.missed());
}

// Deduplication upstream means a repeat should not reach us, but if one does it is not a
// transition either way.
static void a_repeat_is_ignored(void) {
  ReceiveStats stats;
  stats.observe(5);
  stats.observe(6);
  stats.observe(6);
  TEST_ASSERT_EQUAL_UINT8(1, stats.heard());
  TEST_ASSERT_EQUAL_UINT8(0, stats.missed());
}

// Bounded, and weighted to recent behaviour: a bad hour last week must not be diluted
// away by a good week, and the counters must not grow without limit.
static void the_window_stays_bounded_and_recent(void) {
  ReceiveStats stats;
  uint8_t seq = 0;
  for (uint16_t i = 0; i < 500; i++) {
    stats.observe(++seq);
  }
  TEST_ASSERT_TRUE(stats.heard() + stats.missed() <= RECEIVE_WINDOW);
  TEST_ASSERT_EQUAL_UINT8(100, stats.capturePercent());

  // A run of misses now must move the figure, not be swamped by the clean history.
  for (uint16_t i = 0; i < 20; i++) {
    seq = (uint8_t)(seq + 3);
    stats.observe(seq);
  }
  TEST_ASSERT_TRUE(stats.capturePercent() < 80);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(starts_with_nothing_to_report);
  RUN_TEST(the_first_press_is_only_a_reference);
  RUN_TEST(consecutive_presses_are_clean);
  RUN_TEST(a_small_gap_counts_as_missed);
  RUN_TEST(a_large_gap_is_a_new_session_not_a_fault);
  RUN_TEST(the_counter_wraps);
  RUN_TEST(a_repeat_is_ignored);
  RUN_TEST(the_window_stays_bounded_and_recent);
  return UNITY_END();
}
