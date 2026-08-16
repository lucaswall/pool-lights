#pragma once

#include "v2rf.h"

// Builds packets for one device and group. The sequence number is the state that makes
// this a class rather than a function: a receiver uses it to tell a repeat from a fresh
// press, so it has to advance per command and stay put across the repeats of one.
class Encoder {
 public:
  Encoder(uint16_t deviceId, uint8_t group) : _deviceId(deviceId), _group(group) {}

  // out must have room for V2_PACKET_LEN bytes.
  void encode(uint8_t command, uint8_t arg, uint8_t *out) {
    v2Build(out, _deviceId, _group, command, arg, _sequence++);
  }

  uint16_t deviceId() const { return _deviceId; }
  uint8_t group() const { return _group; }

 private:
  uint16_t _deviceId;
  uint8_t _group;
  uint8_t _sequence = 0;
};
