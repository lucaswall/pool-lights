#include "PL1167.h"

#include <string.h>

#include "milight_wire.h"

bool PL1167::begin(const uint8_t *syncword, uint8_t payloadLength) {
  _syncword = syncword;
  // +1 for the length byte the PL1167 puts in front of the payload.
  _payloadLength = (uint8_t)(payloadLength + 1);

  if (!_radio.begin()) {
    return false;
  }
  _radio.setAutoAck(false);       // there is no PL1167 peer to acknowledge us
  _radio.setDataRate(RF24_1MBPS);
  _radio.disableCRC();            // the PL1167 CRC is a different polynomial, done in software
  _radio.setAddressWidth(MILIGHT_SYNCWORD_LEN);
  _radio.setPALevel(RF24_PA_MAX);  // PA+LNA module: anything less wastes the external amp

  return configure();
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

  const uint8_t length = readFrame();
  if (length == 0) {
    return 0;
  }

  // _frame[0] is the PL1167 length byte; the payload follows it.
  if (_frame[0] != length - 1) {
    return 0;
  }
  const uint8_t payloadLength = (uint8_t)(length - 1);
  if (payloadLength > payloadCapacity) {
    return 0;
  }

  memcpy(payload, _frame + 1, payloadLength);
  return payloadLength;
}

uint8_t PL1167::readFrame() {
  uint8_t raw[sizeof(_frame)];
  _radio.read(raw, _frameLength);

  // Re-applying the whole configuration after each read is upstream's workaround for the
  // radio wedging in this abused mode. Removing it stops reception after the first packet.
  configure();

  for (uint8_t i = 0; i < _frameLength; i++) {
    _frame[i] = reverseBits(raw[i]);
  }

  if (_frameLength < 2) {
    return 0;
  }

  const uint8_t bodyLength = (uint8_t)(_frameLength - 2);
  const uint16_t expected = milightCrc(_frame, bodyLength);
  const uint16_t received = (uint16_t)((_frame[bodyLength + 1] << 8) | _frame[bodyLength]);

  return expected == received ? bodyLength : 0;
}

bool PL1167::transmit(uint8_t channel, const uint8_t *payload, uint8_t payloadLength) {
  if (channel != _channel) {
    _channel = channel;
    if (!configure()) {
      return false;
    }
  }

  // The PL1167 length byte precedes the payload and is covered by the CRC.
  uint8_t frame[sizeof(_frame)];
  frame[0] = payloadLength;
  memcpy(frame + 1, payload, payloadLength);
  const uint8_t bodyLength = (uint8_t)(payloadLength + 1);
  const uint16_t crc = milightCrc(frame, bodyLength);

  uint8_t out[sizeof(_frame)];
  for (uint8_t i = 0; i < bodyLength; i++) {
    out[i] = reverseBits(frame[i]);
  }
  out[bodyLength] = reverseBits((uint8_t)(crc & 0xFF));
  out[bodyLength + 1] = reverseBits((uint8_t)(crc >> 8));

  _radio.stopListening();
  return _radio.write(out, (uint8_t)(bodyLength + 2));
}
