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

// The dirty flag is how the publisher knows something changed without diffing.
static void dirty_tracks_real_changes(void) {
  LightState state;
  state.clearDirty();
  TEST_ASSERT_FALSE(state.dirty());

  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  TEST_ASSERT_TRUE(state.dirty());

  state.clearDirty();
  state.apply(cmd(V2_CMD_ON_OFF, v2OnArg(1)));
  TEST_ASSERT_FALSE(state.dirty());
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
  RUN_TEST(dirty_tracks_real_changes);
  return UNITY_END();
}
