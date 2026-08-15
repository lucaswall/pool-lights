#pragma once

#include <stddef.h>
#include <stdint.h>

// PL1167 wire-format helpers. Pure arithmetic, unit tested on the desktop.
// Not named wire.h: that collides with Arduino's Wire.h on case-insensitive filesystems.

// The PL1167 sends bit-reversed relative to how the NRF24 reads out.
inline uint8_t reverseBits(uint8_t b) {
  uint8_t r = 0;
  for (uint8_t i = 0; i < 8; i++) {
    r = (uint8_t)((r << 1) | (b & 1));
    b >>= 1;
  }
  return r;
}

// Folds preamble + syncword + trailer into the NRF24's 5-byte address match. Including
// the 4-bit trailer is what keeps the packet body byte-aligned.
#define MILIGHT_SYNCWORD_LEN 5

inline void milightSyncword(uint16_t syncword0, uint16_t syncword3, uint8_t preamble,
                            uint8_t trailer, uint8_t *out) {
  size_t ix = MILIGHT_SYNCWORD_LEN;
  out[--ix] = reverseBits((uint8_t)(((syncword0 << 4) & 0xF0) | (preamble & 0x0F)));
  out[--ix] = reverseBits((uint8_t)((syncword0 >> 4) & 0xFF));
  out[--ix] = reverseBits((uint8_t)(((syncword0 >> 12) & 0x0F) + ((syncword3 << 4) & 0xF0)));
  out[--ix] = reverseBits((uint8_t)((syncword3 >> 4) & 0xFF));
  out[--ix] = reverseBits((uint8_t)(((syncword3 >> 12) & 0x0F) | ((trailer << 4) & 0xF0)));
}

// CRC-16 poly 0x8408, init 0, LSB-first. Replaces the NRF24's own CRC, which is disabled.
inline uint16_t milightCrc(const uint8_t *data, size_t length) {
  uint16_t state = 0;
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if ((byte ^ state) & 0x01) {
        state = (uint16_t)((state >> 1) ^ 0x8408);
      } else {
        state = (uint16_t)(state >> 1);
      }
      byte >>= 1;
    }
  }
  return state;
}
