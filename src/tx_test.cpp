// Rung 7b — transmit test, built by `make tx`.
//
// Runs a fixed sequence on a timer so the light can be watched from the pool rather than
// the keyboard. The sequence loops, so a missed step comes round again.
//
// Never sends to group 0: repeated ON to group 0 is the pairing command, and a mis-sent
// pair can displace the remote the receiver is bound to.

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "PL1167.h"
#include "milight_wire.h"
#include "secrets.h"
#include "v2rf.h"

static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;

static const uint16_t SYNCWORD_0 = 0x7236;
static const uint16_t SYNCWORD_3 = 0x1809;
static const uint8_t PREAMBLE = 0xAA;
static const uint8_t TRAILER = 0x05;
static const uint8_t CHANNELS[] = {8, 39, 70};

// A real remote repeats each press many times across all three channels. One copy is
// unlikely to land.
static const uint8_t REPEATS = 10;

// Time to walk to the pool before anything happens.
// Enough to plug into a power bank at the light and step back.
static const uint32_t STARTUP_DELAY_MS = 20000;

static RF24 rf24(PIN_CE, PIN_CSN);
static PL1167 radio(rf24);
static uint8_t syncword[MILIGHT_SYNCWORD_LEN];
static uint8_t sequence = 0;

struct Step {
  const char *label;
  uint8_t command;
  uint8_t arg;
  uint32_t holdMs;
};

static const Step SEQUENCE[] = {
  {"ON",             V2_CMD_ON_OFF,     v2OnArg(MILIGHT_GROUP),  6000},
  {"OFF",            V2_CMD_ON_OFF,     v2OffArg(MILIGHT_GROUP), 6000},
  {"ON again",       V2_CMD_ON_OFF,     v2OnArg(MILIGHT_GROUP),  4000},
  {"RED",            V2_CMD_COLOR,      0x00,                    5000},
  {"GREEN",          V2_CMD_COLOR,      0x55,                    5000},
  {"BLUE",           V2_CMD_COLOR,      0xAB,                    5000},
  {"WHITE mode",     V2_CMD_ON_OFF,     V2_ARG_WHITE_MODE,       5000},
  {"brightness 100", V2_CMD_BRIGHTNESS, 100,                     4000},
  {"brightness 10",  V2_CMD_BRIGHTNESS, 10,                      4000},
  {"brightness 100", V2_CMD_BRIGHTNESS, 100,                     4000},
  {"OFF (end)",      V2_CMD_ON_OFF,     v2OffArg(MILIGHT_GROUP), 8000},
};

static void send(const Step &step) {
  uint8_t packet[V2_PACKET_LEN];
  // One sequence number per press, held across the repeats, as the remote does.
  v2Build(packet, MILIGHT_DEVICE_ID, MILIGHT_GROUP, step.command, step.arg, sequence++);

  uint16_t sent = 0, failed = 0;
  for (uint8_t r = 0; r < REPEATS; r++) {
    for (uint8_t c = 0; c < sizeof(CHANNELS) / sizeof(CHANNELS[0]); c++) {
      if (radio.transmit(CHANNELS[c], packet, V2_PACKET_LEN)) {
        sent++;
      } else {
        failed++;
      }
    }
    yield();
  }
  Serial.printf("sent %-16s cmd 0x%02X arg 0x%02X  radio ok %u, failed %u\n",
                step.label, step.command, step.arg, sent, failed);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== pool-lights: transmit test ==="));
  Serial.printf("build   : %s %s\n", __DATE__, __TIME__);
  Serial.printf("target  : group %u\n", MILIGHT_GROUP);

  static_assert(MILIGHT_GROUP != 0, "group 0 broadcasts, and repeated ON to it pairs");

  SPI.begin();
  milightSyncword(SYNCWORD_0, SYNCWORD_3, PREAMBLE, TRAILER, syncword);
  if (!radio.begin(syncword, V2_PACKET_LEN)) {
    Serial.println(F("radio   : FAILED to start"));
    return;
  }

  Serial.printf("starting in %lu s — go and watch the light\n",
                (unsigned long)(STARTUP_DELAY_MS / 1000));
  delay(STARTUP_DELAY_MS);
}

void loop() {
  for (uint8_t i = 0; i < sizeof(SEQUENCE) / sizeof(SEQUENCE[0]); i++) {
    send(SEQUENCE[i]);
    delay(SEQUENCE[i].holdMs);
  }
  Serial.println(F("--- sequence complete, repeating ---\n"));
}
