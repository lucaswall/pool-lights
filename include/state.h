#pragma once

#include "packet.h"

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

  bool dirty() const { return _dirty; }
  void clearDirty() { _dirty = false; }

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
      set(_on, true);
      return;
    }
    // A held OFF is night mode: a dim glow below the normal minimum, not off.
    if (packet.held) {
      set(_on, true);
      set(_night, true);
      return;
    }
    set(_on, packet.argument <= 8);
    clearNight();
  }

  void clearNight() { set(_night, false); }

  static uint8_t clampPercent(uint8_t value) { return value > 100 ? 100 : value; }

  template <typename T>
  void set(T &field, T value) {
    if (field != value) {
      field = value;
      _dirty = true;
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
  bool _dirty = false;
};
