#pragma once

#include <stddef.h>
#include <stdint.h>

// MiLight/MiBoxer "V2" packet obfuscation and layout.
//
// The offset table and xorKey derivation are reverse-engineered constants from
// sidoh/esp8266_milight_hub (MIT), originally henryk/openmili. They cannot be derived
// from first principles. The code around them is ours, and unlike upstream's it is tested.

#define V2_PACKET_LEN 9

// Field positions in a decoded packet. The names come from the offset table's own rows.
enum {
  V2_KEY = 0,
  V2_PROTOCOL = 1,
  V2_ID_HIGH = 2,
  V2_ID_LOW = 3,
  V2_COMMAND = 4,
  V2_ARGUMENT = 5,
  V2_SEQUENCE = 6,
  V2_GROUP = 7,
  V2_CHECKSUM = 8,
};

// The FUT088 is a protocol-0x25 remote in the FUT088/089/092 family.
#define V2_PROTOCOL_FUT089 0x25

// Above this key value the offsets shift by 0x80.
#define V2_OFFSET_JUMP_START 0x54

inline uint8_t v2Offset(uint8_t index, uint8_t key, uint8_t jumpStart) {
  static const uint8_t OFFSETS[8][4] = {
    {0x45, 0x1F, 0x14, 0x5C},  // protocol
    {0x2B, 0xC9, 0xE3, 0x11},  // id high
    {0x6D, 0x5F, 0x8A, 0x2B},  // id low
    {0xAF, 0x03, 0x1D, 0xF3},  // command
    {0x1A, 0xE2, 0xF0, 0xD1},  // argument
    {0x04, 0xD8, 0x71, 0x42},  // sequence
    {0xAF, 0x04, 0xDD, 0x07},  // group
    {0x61, 0x13, 0x38, 0x64},  // checksum
  };
  uint8_t offset = OFFSETS[index - 1][key % 4];
  if (jumpStart > 0 && key >= jumpStart && key < jumpStart + 0x80) {
    offset = (uint8_t)(offset + 0x80);
  }
  return offset;
}

inline uint8_t v2XorKey(uint8_t key) {
  const uint8_t shift = (key & 0x0F) < 0x04 ? 0 : 1;
  const uint8_t x = (uint8_t)((((key & 0xF0) >> 4) + shift + 6) % 8);
  const uint8_t msn = (uint8_t)((((4 + x) ^ 1) & 0x0F) << 4);
  const uint8_t lsn = (uint8_t)((((key & 0x0F) + 4) ^ 2) & 0x0F);
  return (uint8_t)(msn | lsn);
}

inline uint8_t v2EncodeByte(uint8_t byte, uint8_t s1, uint8_t key, uint8_t s2) {
  return (uint8_t)(((uint8_t)(byte + s1) ^ key) + s2);
}

inline uint8_t v2DecodeByte(uint8_t byte, uint8_t s1, uint8_t key, uint8_t s2) {
  return (uint8_t)(((uint8_t)(byte - s2) ^ key) - s1);
}

inline void v2Decode(uint8_t *packet) {
  const uint8_t key = v2XorKey(packet[V2_KEY]);
  for (uint8_t i = 1; i <= 8; i++) {
    packet[i] = v2DecodeByte(packet[i], 0, key,
                             v2Offset(i, packet[V2_KEY], V2_OFFSET_JUMP_START));
  }
}

inline void v2Encode(uint8_t *packet) {
  const uint8_t key = v2XorKey(packet[V2_KEY]);
  uint8_t sum = key;
  for (uint8_t i = 1; i <= 7; i++) {
    sum = (uint8_t)(sum + packet[i]);
    packet[i] = v2EncodeByte(packet[i], 0, key,
                             v2Offset(i, packet[V2_KEY], V2_OFFSET_JUMP_START));
  }
  // The checksum is offset with jumpStart 0, unlike every other byte.
  packet[V2_CHECKSUM] = v2EncodeByte(sum, 2, key, v2Offset(8, packet[V2_KEY], 0));
}

inline uint16_t v2DeviceId(const uint8_t *decoded) {
  return (uint16_t)((decoded[V2_ID_HIGH] << 8) | decoded[V2_ID_LOW]);
}

inline bool v2IsFut089(const uint8_t *decoded) {
  return decoded[V2_PROTOCOL] == V2_PROTOCOL_FUT089;
}
