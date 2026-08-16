#include "PL1167.h"

#include <string.h>

// Well over the ~100 us a 12-byte frame needs at 1 Mbps, and far under the watchdog.
static const uint32_t TX_TIMEOUT_MS = 10;

#include "milight_frame.h"

bool PL1167::begin(const uint8_t *syncword, uint8_t payloadLength) {
  _syncword = syncword;
  // +1 for the length byte the PL1167 puts in front of the payload.
  _payloadLength = (uint8_t)(payloadLength + 1);

  // Configure unconditionally. RF24::begin() writes the acking, CRC-appending defaults
  // before the readback that decides its return value, so bailing out early would leave
  // a marginal chip in a state that rejects every MiLight frame.
  const bool started = _radio.begin();
  _radio.setAutoAck(false);       // there is no PL1167 peer to acknowledge us
  _radio.setDataRate(RF24_1MBPS);
  _radio.disableCRC();            // the PL1167 CRC is a different polynomial, done in software
  _radio.setAddressWidth(MILIGHT_SYNCWORD_LEN);
  _radio.setPALevel(RF24_PA_MAX);  // PA+LNA module with decoupling fitted

  return started && configure();
}

bool PL1167::configure() {
  // +2 for the CRC that follows the payload.
  _frameLength = (uint8_t)(_payloadLength + 2);
  if (_frameLength > sizeof(_frame)) {
    return false;
  }

  _radio.openReadingPipe(1, _syncword);
  _radio.openWritingPipe(_syncword);
  _radio.setPayloadSize(_frameLength);
  // Channel numbers are quoted relative to 2400 MHz; the NRF24 counts from 2402.
  _radio.setChannel((uint8_t)(2 + _channel));
  return true;
}

bool PL1167::failureDetected() {
  if (!_radio.failureDetected) {
    return false;
  }
  _radio.failureDetected = 0;
  return true;
}

uint8_t PL1167::receive(uint8_t channel, uint8_t *payload, uint8_t payloadCapacity) {
  if (channel != _channel) {
    _channel = channel;
    if (!configure()) {
      return 0;
    }
  }

  _radio.startListening();
  if (!_radio.available()) {
    return 0;
  }

  uint8_t raw[sizeof(_frame)];
  _radio.read(raw, _frameLength);

  // Re-applying the whole configuration after each read is upstream's workaround for the
  // radio wedging in this abused mode. Removing it stops reception after the first packet.
  configure();

  return milightParseFrame(raw, _frameLength, payload, payloadCapacity);
}

bool PL1167::transmit(uint8_t channel, const uint8_t *payload, uint8_t payloadLength) {
  if (channel != _channel) {
    _channel = channel;
    if (!configure()) {
      return false;
    }
  }

  uint8_t out[sizeof(_frame)];
  const uint8_t frameLength = milightFrame(payload, payloadLength, out);

  // Not RF24::write(): with FAILURE_HANDLING compiled in, its error path spins on a
  // status that never changes and never yields, so a brownout mid-burst takes the board
  // to a watchdog reset. startFastWrite + a bounded txStandBy cannot hang, and its return
  // is a real signal — the FIFO only empties when a packet has been clocked out.
  _radio.stopListening();
  _radio.startFastWrite(out, frameLength, false);
  return _radio.txStandBy(TX_TIMEOUT_MS);
}
