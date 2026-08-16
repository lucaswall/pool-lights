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

// FUT088 commands. See docs/milight-notes.md for the full map.
#define V2_CMD_ON_OFF 0x01
#define V2_CMD_COLOR 0x02
#define V2_CMD_BRIGHTNESS 0x05
#define V2_CMD_MODE 0x06
#define V2_CMD_SAT_KELVIN 0x07
#define V2_ARG_WHITE_MODE 0x14
#define V2_ARG_SPEED_UP 0x12
#define V2_ARG_SPEED_DOWN 0x13

// Bit 7 marks a held key; the argument still says which one.
#define V2_HELD 0x80

inline uint8_t v2BaseCommand(uint8_t command) { return command & 0x7F; }
inline bool v2IsHeld(uint8_t command) { return (command & V2_HELD) != 0; }

// numGroups is 8 for this family, so OFF is the group plus nine.
inline uint8_t v2OnArg(uint8_t group) { return group; }
inline uint8_t v2OffArg(uint8_t group) { return (uint8_t)(group + 9); }

// Builds an encoded, ready-to-send packet. Key 0x00: the remotes vary it, but the
// receiver derives everything from it, so a constant is fine and keeps this reproducible.
inline void v2Build(uint8_t *packet, uint16_t deviceId, uint8_t group, uint8_t command,
                    uint8_t arg, uint8_t sequence) {
  packet[V2_KEY] = 0x00;
  packet[V2_PROTOCOL] = V2_PROTOCOL_FUT089;
  packet[V2_ID_HIGH] = (uint8_t)(deviceId >> 8);
  packet[V2_ID_LOW] = (uint8_t)(deviceId & 0xFF);
  packet[V2_COMMAND] = command;
  packet[V2_ARGUMENT] = arg;
  packet[V2_SEQUENCE] = sequence;
  packet[V2_GROUP] = group;
  packet[V2_CHECKSUM] = 0;
  v2Encode(packet);
}

// Names a command for humans reading a sniff. Masks the held bit: without that every
// long press reads as an unknown command.
inline const char *v2CommandName(uint8_t command, uint8_t arg) {
  switch (v2BaseCommand(command)) {
    case V2_CMD_ON_OFF:
      if (arg == V2_ARG_WHITE_MODE) return "white mode";
      if (arg == V2_ARG_SPEED_UP) return "mode speed up";
      if (arg == V2_ARG_SPEED_DOWN) return "mode speed down";
      return arg > 8 ? "off" : "on";
    case V2_CMD_COLOR: return "colour";
    case V2_CMD_BRIGHTNESS: return "brightness";
    case V2_CMD_MODE: return "mode";
    case V2_CMD_SAT_KELVIN: return "saturation/kelvin";
  }
  return "unknown";
}

inline uint16_t v2DeviceId(const uint8_t *decoded) {
  return (uint16_t)((decoded[V2_ID_HIGH] << 8) | decoded[V2_ID_LOW]);
}

inline bool v2IsFut089(const uint8_t *decoded) {
  return decoded[V2_PROTOCOL] == V2_PROTOCOL_FUT089;
}
