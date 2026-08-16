#pragma once

#include <RF24.h>

#include "PL1167.h"
#include "control.h"
#include "milight_wire.h"
#include "packet_queue.h"

// Owns the radio and arbitrates it, because it is half-duplex and two jobs want it: we
// listen almost all the time so presses on the physical remote reach our state, and we
// interrupt that to transmit a command. Nothing else touches the PL1167.
class RadioLink : public PacketSink {
 public:
  RadioLink(uint8_t cePin, uint8_t csnPin) : _rf24(cePin, csnPin), _pl1167(_rf24) {}

  bool begin();

  // Queues one packet. Sent on a later loop(), which keeps callers off the radio's timing
  // and stops a burst happening inside a web request or an MQTT callback. Queued rather
  // than held in one slot: a single Home Assistant message can carry colour, brightness
  // and power, and each would otherwise overwrite the one before it.
  void send(const uint8_t *packet) override;

  void loop();

  // Pops a packet overheard since the last call. False if there was nothing new.
  bool take(Packet *out);

 private:
  void transmitNext();
  void listen();

  RF24 _rf24;
  PL1167 _pl1167;
  uint8_t _syncword[MILIGHT_SYNCWORD_LEN] = {0};

  PacketQueue _outgoing;

  Packet _received;
  bool _hasReceived = false;
  uint8_t _lastRaw[V2_PACKET_LEN] = {0};

  uint8_t _channelIx = 0;
  uint32_t _lastHop = 0;
};
