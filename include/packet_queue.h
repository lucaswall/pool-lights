#pragma once

#include <string.h>

#include "v2rf.h"

// Commands waiting for the radio. A single slot is not enough: one Home Assistant message
// can carry colour, brightness and power at once, and each would overwrite the last.
//
// Eight is well clear of the four a single message can produce, and costs 72 bytes.
#define PACKET_QUEUE_LEN 8

class PacketQueue {
 public:
  void push(const uint8_t *packet) {
    memcpy(_slots[_head], packet, V2_PACKET_LEN);
    _head = next(_head);
    if (_count == PACKET_QUEUE_LEN) {
      // Full: drop the oldest rather than the newest, which is the one that reflects what
      // somebody just asked for.
      _tail = next(_tail);
    } else {
      _count++;
    }
  }

  bool pop(uint8_t *out) {
    if (_count == 0) {
      return false;
    }
    memcpy(out, _slots[_tail], V2_PACKET_LEN);
    _tail = next(_tail);
    _count--;
    return true;
  }

  bool empty() const { return _count == 0; }

 private:
  static uint8_t next(uint8_t index) {
    return (uint8_t)((index + 1) % PACKET_QUEUE_LEN);
  }

  uint8_t _slots[PACKET_QUEUE_LEN][V2_PACKET_LEN] = {{0}};
  uint8_t _head = 0;
  uint8_t _tail = 0;
  uint8_t _count = 0;
};
