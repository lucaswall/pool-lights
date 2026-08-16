#include "radio_link.h"

#include <SPI.h>
#include <string.h>

#include "log.h"
#include "timing.h"

// The rgb+cct radio config, which is what the FUT088/089/092 family uses.
static const uint16_t SYNCWORD_0 = 0x7236;
static const uint16_t SYNCWORD_3 = 0x1809;
static const uint8_t PREAMBLE = 0xAA;
static const uint8_t TRAILER = 0x05;
static const uint8_t CHANNELS[] = {8, 39, 70};
static const uint8_t CHANNEL_COUNT = sizeof(CHANNELS) / sizeof(CHANNELS[0]);

// A remote sends every press on all three channels many times over. Match that: one copy
// is unlikely to land, and the receiver samples rather than listening continuously.
static const uint8_t REPEATS = 50;
static const uint8_t REPEATS_PER_BURST = 10;

// Listening on one channel at a time; which one is not knowable in advance, so rotate.
static const uint32_t CHANNEL_DWELL_MS = 2000;

bool RadioLink::begin() {
  SPI.begin();
  milightSyncword(SYNCWORD_0, SYNCWORD_3, PREAMBLE, TRAILER, _syncword);
  return _pl1167.begin(_syncword, V2_PACKET_LEN);
}

void RadioLink::send(const uint8_t *packet) {
  if (!_outgoing.push(packet)) {
    logLine("radio     : command queue full, oldest dropped");
  }
}

void RadioLink::loop() {
  if (!_outgoing.empty()) {
    transmitNext();
    return;
  }
  listen();
}

// One packet per call, so a queue of commands does not block the loop — and the web
// server and MQTT keep getting served between bursts.
void RadioLink::transmitNext() {
  uint8_t packet[V2_PACKET_LEN];
  if (!_outgoing.pop(packet)) {
    return;
  }

  // Logged for the same reason overheard packets are: a command that never left was
  // indistinguishable from one that did, which hid a queue overwriting itself.
  Packet decoded;
  if (decodePacket(packet, &decoded)) {
    logLine("sent      : %s (0x%02X arg 0x%02X)",
                  v2CommandName(decoded.command, decoded.argument), decoded.command,
                  decoded.argument);
  }

  for (uint8_t r = 0; r < REPEATS; r++) {
    for (uint8_t c = 0; c < CHANNEL_COUNT; c++) {
      _pl1167.transmit(CHANNELS[c], packet, V2_PACKET_LEN);
    }
    if ((r + 1) % REPEATS_PER_BURST == 0) {
      delay(2);
    }
    yield();
  }
}

void RadioLink::listen() {
  const uint32_t now = millis();
  if (elapsed(now, _lastHop, CHANNEL_DWELL_MS)) {
    _lastHop = now;
    _channelIx = (uint8_t)((_channelIx + 1) % CHANNEL_COUNT);
  }

  uint8_t raw[V2_PACKET_LEN];
  if (_pl1167.receive(CHANNELS[_channelIx], raw, sizeof(raw)) != V2_PACKET_LEN) {
    return;
  }

  // A press is repeated dozens of times. Report the first copy only, or every consumer
  // downstream has to do the same de-duplication.
  if (memcmp(raw, _lastRaw, V2_PACKET_LEN) == 0) {
    return;
  }
  memcpy(_lastRaw, raw, V2_PACKET_LEN);

  if (decodePacket(raw, &_received)) {
    _hasReceived = true;
  }
}

bool RadioLink::take(Packet *out) {
  if (!_hasReceived) {
    return false;
  }
  *out = _received;
  _hasReceived = false;
  return true;
}
