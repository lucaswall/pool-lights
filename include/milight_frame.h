#pragma once

#include "milight_wire.h"

// The PL1167 frame around a payload:
//
//   [length] [payload...] [crc lo] [crc hi]     — every byte bit-reversed on air
//
// The length byte is covered by the CRC; the CRC bytes are not. Pure, so the framing that
// composes reverseBits and milightCrc is testable without a radio — previously only the
// two halves were, and never their composition.

// Total frame bytes for a payload of this length.
#define MILIGHT_FRAME_LEN(payloadLength) ((payloadLength) + 3)

// Writes MILIGHT_FRAME_LEN(payloadLength) bytes to out. Returns that length.
inline uint8_t milightFrame(const uint8_t *payload, uint8_t payloadLength, uint8_t *out) {
  const uint8_t bodyLength = (uint8_t)(payloadLength + 1);

  uint8_t body[32];
  body[0] = payloadLength;
  memcpy(body + 1, payload, payloadLength);
  const uint16_t crc = milightCrc(body, bodyLength);

  for (uint8_t i = 0; i < bodyLength; i++) {
    out[i] = reverseBits(body[i]);
  }
  out[bodyLength] = reverseBits((uint8_t)(crc & 0xFF));
  out[bodyLength + 1] = reverseBits((uint8_t)(crc >> 8));
  return (uint8_t)(bodyLength + 2);
}

// Reverses a received frame, checks the CRC and the length byte, and copies the payload
// out. Returns the payload length, or 0 if the frame is not one of ours.
inline uint8_t milightParseFrame(const uint8_t *raw, uint8_t frameLength, uint8_t *payload,
                                 uint8_t payloadCapacity) {
  if (frameLength < 3 || frameLength > 32) {
    return 0;
  }

  uint8_t body[32];
  for (uint8_t i = 0; i < frameLength; i++) {
    body[i] = reverseBits(raw[i]);
  }

  const uint8_t bodyLength = (uint8_t)(frameLength - 2);
  const uint16_t expected = milightCrc(body, bodyLength);
  const uint16_t received = (uint16_t)((body[bodyLength + 1] << 8) | body[bodyLength]);
  if (expected != received) {
    return 0;
  }

  // The leading length byte must agree with what the radio was told to read, or this is
  // a frame from something else that happened to match the address and CRC.
  const uint8_t payloadLength = (uint8_t)(bodyLength - 1);
  if (body[0] != payloadLength || payloadLength > payloadCapacity) {
    return 0;
  }

  memcpy(payload, body + 1, payloadLength);
  return payloadLength;
}
