#include <unity.h>

#include "units.h"

void setUp(void) {}
void tearDown(void) {}

// Home Assistant sends colour as an RGB triple; the protocol carries one hue byte.
static void primaries_map_to_the_protocol_hues(void) {
  TEST_ASSERT_EQUAL_UINT8(0, rgbToHue(255, 0, 0));
  TEST_ASSERT_UINT8_WITHIN(1, 85, rgbToHue(0, 255, 0));
  TEST_ASSERT_UINT8_WITHIN(1, 170, rgbToHue(0, 0, 255));
}

// Grey has no hue at all. Any answer is arguable; an unpinned one is not.
static void greys_are_hue_zero(void) {
  TEST_ASSERT_EQUAL_UINT8(0, rgbToHue(128, 128, 128));
  TEST_ASSERT_EQUAL_UINT8(0, rgbToHue(0, 0, 0));
}

static void hue_expands_back_to_primaries(void) {
  uint8_t r, g, b;
  hueToRgb(0, &r, &g, &b);
  TEST_ASSERT_EQUAL_UINT8(255, r);
  TEST_ASSERT_EQUAL_UINT8(0, b);

  hueToRgb(85, &r, &g, &b);
  TEST_ASSERT_EQUAL_UINT8(255, g);

  hueToRgb(170, &r, &g, &b);
  TEST_ASSERT_EQUAL_UINT8(255, b);
}

// The wrap at the top of the circle is where off-by-ones live.
static void round_trip_survives_the_wrap(void) {
  for (uint16_t h = 0; h <= 255; h++) {
    uint8_t r, g, b;
    hueToRgb((uint8_t)h, &r, &g, &b);
    const int diff = (int)rgbToHue(r, g, b) - (int)h;
    const int wrapped = diff > 128 ? diff - 256 : (diff < -128 ? diff + 256 : diff);
    TEST_ASSERT_TRUE(wrapped <= 2 && wrapped >= -2);
  }
}

// Mireds run opposite to kelvin: 153 is cool daylight, 500 is warm.
static void mireds_span_the_protocol_scale(void) {
  TEST_ASSERT_EQUAL_UINT8(0, miredsToProtocol(HA_MIRED_MIN));
  TEST_ASSERT_EQUAL_UINT8(100, miredsToProtocol(HA_MIRED_MAX));
  TEST_ASSERT_UINT8_WITHIN(2, 50, miredsToProtocol((HA_MIRED_MIN + HA_MIRED_MAX) / 2));
}

static void mireds_clamp_outside_the_declared_range(void) {
  TEST_ASSERT_EQUAL_UINT8(0, miredsToProtocol(10));
  TEST_ASSERT_EQUAL_UINT8(100, miredsToProtocol(2000));
}

static void protocol_maps_back_to_mireds(void) {
  TEST_ASSERT_EQUAL_UINT16(HA_MIRED_MIN, protocolToMireds(0));
  TEST_ASSERT_EQUAL_UINT16(HA_MIRED_MAX, protocolToMireds(100));
}

// Home Assistant's colour wheel has a desaturated centre, so "white" arrives as an RGB
// triple. Hue alone cannot express it — saturation is what distinguishes white from red.
static void saturation_from_rgb(void) {
  TEST_ASSERT_EQUAL_UINT8(100, rgbSaturation(255, 0, 0));
  TEST_ASSERT_EQUAL_UINT8(0, rgbSaturation(255, 255, 255));
  TEST_ASSERT_EQUAL_UINT8(0, rgbSaturation(0, 0, 0));
  TEST_ASSERT_UINT8_WITHIN(2, 50, rgbSaturation(255, 128, 128));
}

// B4: the clamped endpoints cannot tell a correct mapping from an inverted one. These
// interior points can, in both directions.
static void mireds_interior_points_are_pinned(void) {
  TEST_ASSERT_EQUAL_UINT8(14, miredsToProtocol(200));
  TEST_ASSERT_EQUAL_UINT16(326, protocolToMireds(50));
}

static void mireds_round_trip(void) {
  for (uint8_t v = 0; v <= 100; v++) {
    TEST_ASSERT_EQUAL_UINT8(v, miredsToProtocol(protocolToMireds(v)));
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(primaries_map_to_the_protocol_hues);
  RUN_TEST(greys_are_hue_zero);
  RUN_TEST(hue_expands_back_to_primaries);
  RUN_TEST(round_trip_survives_the_wrap);
  RUN_TEST(mireds_span_the_protocol_scale);
  RUN_TEST(mireds_clamp_outside_the_declared_range);
  RUN_TEST(protocol_maps_back_to_mireds);
  RUN_TEST(saturation_from_rgb);
  RUN_TEST(mireds_interior_points_are_pinned);
  RUN_TEST(mireds_round_trip);
  return UNITY_END();
}
