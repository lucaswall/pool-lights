#pragma once

#include "packet.h"

// The documented durations of the two sleep timers on the S- and S+ keys.
#define SLEEP_SHORT_MS 60000UL
#define SLEEP_LONG_MS 600000UL

enum ColourMode {
  COLOUR_MODE_RGB,
  COLOUR_MODE_WHITE,
};

// What the light is currently doing, as far as we can tell.
//
// The protocol is one-way: the receiver never reports anything. State is therefore
// inferred from traffic, and apply() is the only way in — so a command we sent and a
// button someone pressed on the physical remote follow the same path and cannot drift.
class LightState {
 public:
  void apply(const Packet &packet) {
    switch (packet.command) {
      case V2_CMD_ON_OFF:
        applyOnOff(packet);
        break;
      case V2_CMD_COLOR:
        set(_mode, COLOUR_MODE_RGB);
        set(_hue, packet.argument);
        clearNight();
        break;
      case V2_CMD_BRIGHTNESS:
        set(_brightness, clampPercent(packet.argument));
        clearNight();
        break;
      case V2_CMD_MODE:
        set(_effect, packet.argument);
        clearNight();
        break;
      case V2_CMD_SAT_KELVIN:
        // One bar on the remote, two meanings depending on the mode the light is in.
        if (_mode == COLOUR_MODE_WHITE) {
          set(_kelvin, clampPercent(packet.argument));
        } else {
          set(_saturation, clampPercent(packet.argument));
        }
        clearNight();
        break;
      default:
        break;
    }
  }

  bool on() const { return _on; }
  bool night() const { return _night; }
  uint8_t brightness() const { return _brightness; }
  ColourMode mode() const { return _mode; }
  uint8_t hue() const { return _hue; }
  uint8_t saturation() const { return _saturation; }
  uint8_t kelvin() const { return _kelvin; }
  uint8_t effect() const { return _effect; }

  // Readers keep their own last-seen value, so any number of them can tell whether they
  // are behind without competing for a single flag.
  uint32_t version() const { return _version; }

  // Drives the sleep timers armed by a held speed key. Time is passed in rather than read
  // here, so this stays pure. Safe to call every loop.
  void tick(uint32_t nowMs) {
    if (_timerMs == 0) {
      return;
    }
    if (!_timerStarted) {
      _timerStarted = true;
      _timerStartMs = nowMs;
      return;
    }
    if ((uint32_t)(nowMs - _timerStartMs) >= _timerMs) {
      cancelTimer();
      set(_on, false);
      clearNight();
    }
  }

 private:
  void applyOnOff(const Packet &packet) {
    // These arguments are all above the group range, so testing the group arithmetic
    // first would read white mode and both speed keys as OFF.
    if (packet.argument == V2_ARG_WHITE_MODE) {
      set(_mode, COLOUR_MODE_WHITE);
      set(_on, true);
      clearNight();
      return;
    }
    if (packet.argument == V2_ARG_SPEED_UP || packet.argument == V2_ARG_SPEED_DOWN) {
      // Held, these are sleep timers rather than speed adjustments: the light switches
      // itself off after the interval and the protocol never tells us it happened.
      if (packet.held) {
        _timerMs = packet.argument == V2_ARG_SPEED_DOWN ? SLEEP_SHORT_MS : SLEEP_LONG_MS;
        _timerStarted = false;
      }
      set(_on, true);
      return;
    }
    // Night mode is a held OFF specifically. Testing `held` alone here — which is what
    // upstream does — also swallows a held ON, whose effect is undocumented.
    if (packet.held && packet.argument > 8) {
      set(_on, true);
      set(_night, true);
      cancelTimer();
      return;
    }
    set(_on, packet.argument <= 8);
    clearNight();
    cancelTimer();   // pressing on or off cancels a running sleep timer
  }

  void cancelTimer() {
    _timerMs = 0;
    _timerStarted = false;
  }

  void clearNight() { set(_night, false); }

  static uint8_t clampPercent(uint8_t value) { return value > 100 ? 100 : value; }

  template <typename T>
  void set(T &field, T value) {
    if (field != value) {
      field = value;
      _version++;
    }
  }

  bool _on = false;
  bool _night = false;
  uint8_t _brightness = 100;
  ColourMode _mode = COLOUR_MODE_WHITE;
  uint8_t _hue = 0;
  uint8_t _saturation = 100;
  uint8_t _kelvin = 50;
  uint8_t _effect = 0;
  uint32_t _version = 0;
  uint32_t _timerMs = 0;
  uint32_t _timerStartMs = 0;
  bool _timerStarted = false;
};
