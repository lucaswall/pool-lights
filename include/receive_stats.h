#pragma once

#include <stdint.h>

// How much of the physical remote's traffic we are actually hearing.
//
// The remote advances its sequence byte once per press, so a gap in the sequences we
// received is presses that happened and we missed. That makes the figure free: no ground
// truth, nobody counting button presses.
//
// Two things it is not. It measures the direction we can observe — what reaches us —
// while the direction that matters is whether our transmissions reach the light, which a
// one-way protocol can never tell us. And it is silent when nobody touches the remote,
// which looks exactly like perfect. Treat it as a diagnostic to consult, not an alarm.

// Roughly how many transitions the figure reflects. Halving at the limit keeps it bounded
// and weighted to recent behaviour, so a bad hour is not diluted away by a good week.
#define RECEIVE_WINDOW 64

// Beyond this, a gap is someone using the remote in a session we were not part of — while
// we were rebooting, or hours earlier — rather than reception we lost.
#define RECEIVE_MAX_GAP 5

class ReceiveStats {
 public:
  // Called once per distinct press received, with its sequence byte.
  void observe(uint8_t sequence) {
    if (!_haveLast) {
      _haveLast = true;
      _last = sequence;
      return;
    }

    const uint8_t step = (uint8_t)(sequence - _last);
    _last = sequence;

    if (step == 0) {
      return;   // a repeat that survived de-duplication: not a transition
    }
    if (step > RECEIVE_MAX_GAP) {
      return;   // a separate session; resynchronise without scoring it
    }

    _heard++;
    _missed = (uint8_t)(_missed + (step - 1));

    if ((uint16_t)_heard + _missed >= RECEIVE_WINDOW) {
      _heard = (uint8_t)(_heard / 2);
      _missed = (uint8_t)(_missed / 2);
    }
  }

  bool hasData() const { return _heard + _missed > 0; }
  uint8_t heard() const { return _heard; }
  uint8_t missed() const { return _missed; }

  uint8_t capturePercent() const {
    const uint16_t total = (uint16_t)_heard + _missed;
    return total == 0 ? 100 : (uint8_t)(((uint32_t)_heard * 100 + total / 2) / total);
  }

 private:
  uint8_t _last = 0;
  bool _haveLast = false;
  uint8_t _heard = 0;
  uint8_t _missed = 0;
};
