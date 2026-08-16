#pragma once

#include <string.h>

#include "v2rf.h"

// A decoded command as a value. `held` is split out of the command byte so nothing
// downstream has to remember to mask bit 7 — the mistake upstream's decoder makes.
struct Packet {
  uint16_t deviceId;
  uint8_t group;
  uint8_t command;
  uint8_t argument;
  uint8_t sequence;
  bool held;
};

// raw is V2_PACKET_LEN encoded bytes. False means it is not a packet we understand.
inline bool decodePacket(const uint8_t *raw, Packet *out) {
  uint8_t decoded[V2_PACKET_LEN];
  memcpy(decoded, raw, V2_PACKET_LEN);
  v2Decode(decoded);

  if (!v2IsFut089(decoded)) {
    return false;
  }

  out->deviceId = v2DeviceId(decoded);
  out->group = decoded[V2_GROUP];
  out->command = v2BaseCommand(decoded[V2_COMMAND]);
  out->argument = decoded[V2_ARGUMENT];
  out->sequence = decoded[V2_SEQUENCE];
  out->held = v2IsHeld(decoded[V2_COMMAND]);
  return true;
}
