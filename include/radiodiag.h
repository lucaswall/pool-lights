#pragma once

#include <stdint.h>

enum RadioFault {
  RADIO_OK,
  RADIO_MISO_LOW,        // all zeroes: unpowered, no shared ground, or MISO unwired
  RADIO_MISO_FLOATING,   // all ones: nothing driving the bus, no chip responding
  RADIO_UNSTABLE,        // wrong value: bus works but is marginal, usually SPI too fast
};

// Every one of these is a wiring or power fault, never software — which is the whole
// point of testing the radio before any protocol code exists to blame.
inline RadioFault diagnose(uint8_t wrote, uint8_t readBack) {
  if (readBack == wrote) {
    return RADIO_OK;
  }
  if (readBack == 0x00) {
    return RADIO_MISO_LOW;
  }
  if (readBack == 0xff) {
    return RADIO_MISO_FLOATING;
  }
  return RADIO_UNSTABLE;
}

inline const char *faultName(RadioFault f) {
  switch (f) {
    case RADIO_OK:             return "ok";
    case RADIO_MISO_LOW:       return "MISO stuck low — check 3V3, GND and the MISO lead";
    case RADIO_MISO_FLOATING:  return "MISO floating — no chip answering, check CSN and SCK";
    case RADIO_UNSTABLE:       return "readback corrupted — SPI marginal, try a lower clock";
  }
  return "unknown";
}
