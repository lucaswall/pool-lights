#pragma once

// PL1167 transceiver emulated on an NRF24L01+.
//
// Derived from sidoh/esp8266_milight_hub (MIT), itself from henryk/openmili.
//
// Frame, bit-reversed on air:
//   Preamble (8) | Syncword (32) | Trailer (4) | Length (8) | Payload | CRC (16)
// The NRF24 cannot express that, so preamble+syncword+trailer become its 5-byte address
// match and the CRC is checked in software.

#include <RF24.h>
#include <stdint.h>

class PL1167 {
 public:
  explicit PL1167(RF24 &radio) : _radio(radio) {}

  // syncword must be MILIGHT_SYNCWORD_LEN bytes and outlive this object.
  bool begin(const uint8_t *syncword, uint8_t payloadLength);

  // Listens on `channel`. Returns the payload length, or 0 if nothing valid arrived.
  // Payload excludes the leading length byte and the trailing CRC.
  uint8_t receive(uint8_t channel, uint8_t *payload, uint8_t payloadCapacity);

  // Sends one copy. Callers repeat across channels the way a remote does.
  // False means the radio itself reported the transmission failed.
  bool transmit(uint8_t channel, const uint8_t *payload, uint8_t payloadLength);

  // RF24 raises this when the chip stops answering at all. Reads and clears it. Unlike a
  // transmit timeout it also catches a radio that has wedged while listening, which
  // otherwise looks exactly like a quiet room.
  bool failureDetected();

 private:
  bool configure();

  RF24 &_radio;
  const uint8_t *_syncword = nullptr;
  uint8_t _payloadLength = 0;
  uint8_t _frameLength = 0;
  uint8_t _channel = 0xff;
  uint8_t _frame[32] = {0};   // sized reference for the read buffer
};
