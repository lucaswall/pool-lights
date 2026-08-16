#pragma once

#include "encoder.h"
#include "packet.h"
#include "state.h"

// Where commands go. RadioLink implements it on hardware; tests inject a fake, which is
// what keeps everything above this line testable without a radio.
struct PacketSink {
  virtual ~PacketSink() {}
  // False means the packet was discarded rather than queued, so the caller must not
  // record its effect: optimistic state plus a lossy queue is drift the one-way protocol
  // has no way to correct.
  virtual bool send(const uint8_t *packet) = 0;
};

// The facade. Speaks intent and protocol units — deliberately knows nothing about MQTT,
// HTTP or Home Assistant, so those can change without touching any of this.
class Control {
 public:
  Control(PacketSink &sink, uint16_t deviceId, uint8_t group)
      : _sink(sink), _encoder(deviceId, group) {}

  void turnOn() { issue(V2_CMD_ON_OFF, v2OnArg(_encoder.group())); }
  void turnOff() { issue(V2_CMD_ON_OFF, v2OffArg(_encoder.group())); }
  void setWhite() { issue(V2_CMD_ON_OFF, V2_ARG_WHITE_MODE); }
  void setHue(uint8_t hue) { issue(V2_CMD_COLOR, hue); }
  void setBrightness(uint8_t percent) { issue(V2_CMD_BRIGHTNESS, clampPercent(percent)); }
  void setSaturationOrKelvin(uint8_t percent) {
    issue(V2_CMD_SAT_KELVIN, clampPercent(percent));
  }
  void setEffect(uint8_t effect) {
    issue(V2_CMD_MODE, effect > V2_EFFECT_MAX ? V2_EFFECT_MAX : effect);
  }

  // The speed keys ride on the on/off command with their own arguments rather than being
  // commands of their own. They adjust the running effect, not the power state.
  void effectFaster() { issue(V2_CMD_ON_OFF, V2_ARG_SPEED_UP); }
  void effectSlower() { issue(V2_CMD_ON_OFF, V2_ARG_SPEED_DOWN); }

  // Traffic overheard from the physical remote. Never transmits.
  void onReceived(const Packet &packet) {
    if (packet.deviceId != _encoder.deviceId() || packet.group != _encoder.group()) {
      return;
    }
    _state.apply(packet);
  }

  void tick(uint32_t nowMs) { _state.tick(nowMs); }

  LightState &state() { return _state; }
  const LightState &state() const { return _state; }

 private:
  static uint8_t clampPercent(uint8_t value) { return value > 100 ? 100 : value; }

  void issue(uint8_t command, uint8_t arg) {
    uint8_t raw[V2_PACKET_LEN];
    _encoder.encode(command, arg, raw);
    if (!_sink.send(raw)) {
      return;
    }

    // Our own commands move state through the same path as overheard ones.
    Packet packet;
    if (decodePacket(raw, &packet)) {
      _state.apply(packet);
    }
  }

  PacketSink &_sink;
  Encoder _encoder;
  LightState _state;
};
