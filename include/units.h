#pragma once

#include <stdint.h>

// Conversions between what Home Assistant speaks and what the protocol carries. The web
// UI needed none of these — its hue slider is the protocol's own 0-255 byte — but Home
// Assistant insists on RGB triples and mireds, so the lossy edges live here where they
// can be tested rather than scattered through the MQTT layer.

// The range we advertise in the discovery payload. Mireds run opposite to kelvin: 153 is
// cool daylight (~6500 K), 500 is warm (~2000 K).
#define HA_MIRED_MIN 153
#define HA_MIRED_MAX 500

// Hue only. Saturation and value are carried by separate commands, so an RGB triple's
// brightness and washout are deliberately discarded here.
inline uint8_t rgbToHue(uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  const uint8_t min = r < g ? (r < b ? r : b) : (g < b ? g : b);
  const int delta = (int)max - (int)min;
  if (delta == 0) {
    return 0;   // grey: no hue exists, so pick one rather than leave it undefined
  }

  int degrees;
  if (max == r) {
    degrees = 60 * (((int)g - (int)b) * 256 / delta) / 256;
  } else if (max == g) {
    degrees = 120 + 60 * (((int)b - (int)r) * 256 / delta) / 256;
  } else {
    degrees = 240 + 60 * (((int)r - (int)g) * 256 / delta) / 256;
  }
  if (degrees < 0) {
    degrees += 360;
  }
  return (uint8_t)((degrees * 255 + 180) / 360);
}

inline void hueToRgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
  // The modulo is load-bearing: hue 255 scales to exactly 360, and without wrapping it
  // to 0 the top of the circle lands in the magenta ramp instead of back on red.
  const int degrees = (((int)hue * 360) / 255) % 360;
  const int sector = degrees / 60;
  const int within = degrees % 60;
  const uint8_t rise = (uint8_t)((within * 255) / 60);
  const uint8_t fall = (uint8_t)(255 - rise);

  switch (sector) {
    case 0:  *r = 255;  *g = rise; *b = 0;    break;
    case 1:  *r = fall; *g = 255;  *b = 0;    break;
    case 2:  *r = 0;    *g = 255;  *b = rise; break;
    case 3:  *r = 0;    *g = fall; *b = 255;  break;
    case 4:  *r = rise; *g = 0;    *b = 255;  break;
    default: *r = 255;  *g = 0;    *b = fall; break;
  }
}

// The one genuinely lossy conversion. The protocol's 0-100 is opaque — it is not kelvin
// and not mireds, just a position on a bar — so this is a linear fit across the range we
// advertise. If the light comes out warm when Home Assistant asks for cool, invert here
// and nowhere else.
inline uint8_t miredsToProtocol(uint16_t mireds) {
  if (mireds <= HA_MIRED_MIN) {
    return 0;
  }
  if (mireds >= HA_MIRED_MAX) {
    return 100;
  }
  return (uint8_t)(((uint32_t)(mireds - HA_MIRED_MIN) * 100 + (HA_MIRED_MAX - HA_MIRED_MIN) / 2) /
                   (HA_MIRED_MAX - HA_MIRED_MIN));
}

inline uint16_t protocolToMireds(uint8_t value) {
  if (value >= 100) {
    return HA_MIRED_MAX;
  }
  return (uint16_t)(HA_MIRED_MIN + ((uint32_t)value * (HA_MIRED_MAX - HA_MIRED_MIN)) / 100);
}
