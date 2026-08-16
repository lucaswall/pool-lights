#include <unity.h>

#include "state.h"

void setUp(void) {}
void tearDown(void) {}

static Packet cmd(uint8_t command, uint8_t arg, bool held = false) {
  Packet p;
  p.deviceId = 0xabcd;
  p.group = 1;
  p.command = command;
  p.argument = arg;
  p.sequence = 0;
  p.held = held;
  return p;
}

static void starts_off(void) {
  LightState state;
  TEST_ASSERT_FALSE(state.on());
}

static void on_and_off(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  TEST_ASSERT_TRUE(state.on());
  state.apply(cmd(V2_CMD_ON_OFF, v2OffArg(1)));
  TEST_ASSERT_FALSE(state.on());
}

// 0x12, 0x13 and 0x14 are all greater than 8, so a decoder that checks the group
// arithmetic first reads white mode and the speed keys as OFF. Upstream has this bug.
static void special_args_are_not_off(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));

  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_WHITE_MODE));
  TEST_ASSERT_TRUE(state.on());
  TEST_ASSERT_EQUAL(COLOUR_MODE_WHITE, state.mode());

  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_UP));
  TEST_ASSERT_TRUE(state.on());
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_DOWN));
  TEST_ASSERT_TRUE(state.on());
}

static void colour_sets_hue_and_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_WHITE_MODE));
  state.apply(cmd(V2_CMD_COLOR, 0x55));
  TEST_ASSERT_EQUAL(COLOUR_MODE_RGB, state.mode());
  TEST_ASSERT_EQUAL_UINT8(0x55, state.hue());
}

static void brightness_is_clamped_to_the_protocol_scale(void) {
  LightState state;
  state.apply(cmd(V2_CMD_BRIGHTNESS, 100));
  TEST_ASSERT_EQUAL_UINT8(100, state.brightness());
  state.apply(cmd(V2_CMD_BRIGHTNESS, 200));
  TEST_ASSERT_EQUAL_UINT8(100, state.brightness());
}

// One bar, two meanings: saturation in colour mode, colour temperature in white mode.
static void sat_kelvin_depends_on_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_COLOR, 0x10));
  state.apply(cmd(V2_CMD_SAT_KELVIN, 40));
  TEST_ASSERT_EQUAL_UINT8(40, state.saturation());

  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_WHITE_MODE));
  state.apply(cmd(V2_CMD_SAT_KELVIN, 70));
  TEST_ASSERT_EQUAL_UINT8(70, state.kelvin());
  TEST_ASSERT_EQUAL_UINT8(40, state.saturation());
}

static void held_off_is_night_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OffArg(1), true));
  TEST_ASSERT_TRUE(state.on());
  TEST_ASSERT_TRUE(state.night());

  state.apply(cmd(V2_CMD_BRIGHTNESS, 50));
  TEST_ASSERT_FALSE(state.night());
}

static void effect_is_tracked(void) {
  LightState state;
  state.apply(cmd(V2_CMD_MODE, 3));
  TEST_ASSERT_EQUAL_UINT8(3, state.effect());
}

// A version counter rather than a dirty flag: the serial log and the web UI both need to
// know whether they are behind, and a single bool cannot serve two readers.
static void version_advances_only_on_real_changes(void) {
  LightState state;
  const uint32_t start = state.version();

  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  const uint32_t afterChange = state.version();
  TEST_ASSERT_TRUE(afterChange > start);

  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  TEST_ASSERT_EQUAL_UINT32(afterChange, state.version());
}

// Two readers tracking their own last-seen version must not interfere.
static void two_readers_are_independent(void) {
  LightState state;
  uint32_t readerA = state.version();
  uint32_t readerB = state.version();

  state.apply(cmd(V2_CMD_BRIGHTNESS, 40));
  TEST_ASSERT_TRUE(state.version() != readerA);
  readerA = state.version();
  TEST_ASSERT_TRUE(state.version() != readerB);
}

// The held bit was consulted only after the special arguments had already returned, so a
// long press of ON — argument = groupId — fell through into the night-mode branch.
static void held_on_is_not_night_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1), true));
  TEST_ASSERT_TRUE(state.on());
  TEST_ASSERT_FALSE(state.night());
}

static void only_a_held_off_is_night_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OffArg(1), true));
  TEST_ASSERT_TRUE(state.night());
}

// Held white mode and held speed keys must not be read as night mode either.
static void held_special_args_are_not_night_mode(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_WHITE_MODE, true));
  TEST_ASSERT_FALSE(state.night());
  TEST_ASSERT_EQUAL(COLOUR_MODE_WHITE, state.mode());
}

// A held speed key is a sleep timer: the light switches itself off later and the protocol
// never says so. Modelled from the documented durations.
static void held_speed_keys_arm_a_sleep_timer(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_DOWN, true));   // 60 s

  state.tick(1000);
  state.tick(59000);
  TEST_ASSERT_TRUE(state.on());
  state.tick(62000);
  TEST_ASSERT_FALSE(state.on());
}

static void the_long_timer_is_ten_minutes(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_UP, true));     // 10 min
  state.tick(0);
  state.tick(9UL * 60 * 1000);
  TEST_ASSERT_TRUE(state.on());
  state.tick(11UL * 60 * 1000);
  TEST_ASSERT_FALSE(state.on());
}

// Pressing on or off cancels a running timer.
static void on_or_off_cancels_the_timer(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_DOWN, true));
  state.tick(0);
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  state.tick(120000);
  TEST_ASSERT_TRUE(state.on());
}

// A short press of a speed key adjusts effect speed and must not arm anything.
static void a_short_speed_press_arms_nothing(void) {
  LightState state;
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  state.apply(cmd(V2_CMD_ON_OFF, V2_ARG_SPEED_DOWN));
  state.tick(0);
  state.tick(120000);
  TEST_ASSERT_TRUE(state.on());
}

// B6: the 8/9 split between an ON argument and an OFF argument is the only thing
// separating them, and every other test uses group 1.
static void group_boundaries(void) {
  LightState low;
  low.apply(cmd(V2_CMD_ON_OFF, v2OnArg(0)));
  TEST_ASSERT_TRUE(low.on());
  low.apply(cmd(V2_CMD_ON_OFF, v2OffArg(0)));
  TEST_ASSERT_FALSE(low.on());

  LightState high;
  high.apply(cmd(V2_CMD_ON_OFF, v2OnArg(8)));    // 0x08
  TEST_ASSERT_TRUE(high.on());
  high.apply(cmd(V2_CMD_ON_OFF, v2OffArg(8)));   // 0x11
  TEST_ASSERT_FALSE(high.on());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(starts_off);
  RUN_TEST(on_and_off);
  RUN_TEST(special_args_are_not_off);
  RUN_TEST(colour_sets_hue_and_mode);
  RUN_TEST(brightness_is_clamped_to_the_protocol_scale);
  RUN_TEST(sat_kelvin_depends_on_mode);
  RUN_TEST(held_off_is_night_mode);
  RUN_TEST(effect_is_tracked);
  RUN_TEST(version_advances_only_on_real_changes);
  RUN_TEST(two_readers_are_independent);
  RUN_TEST(held_on_is_not_night_mode);
  RUN_TEST(only_a_held_off_is_night_mode);
  RUN_TEST(held_special_args_are_not_night_mode);
  RUN_TEST(held_speed_keys_arm_a_sleep_timer);
  RUN_TEST(the_long_timer_is_ten_minutes);
  RUN_TEST(on_or_off_cancels_the_timer);
  RUN_TEST(a_short_speed_press_arms_nothing);
  RUN_TEST(group_boundaries);
  return UNITY_END();
}
