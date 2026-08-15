// Rung 7a — receive-only MiLight sniffer. Built by `make sniff`.
//
// Learns the physical remote's identity by listening. Nothing is transmitted: the binary
// contains no transmit path at all, so no mis-sent pairing command can displace what the
// pool receiver is bound to.
//
// Press buttons on the FUT088 and read the device ID, group and command off the output.
// Capture is unreliable by nature — press repeatedly.

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "PL1167Rx.h"
#include "milight_wire.h"
#include "timing.h"
#include "v2rf.h"

static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;   // not GPIO15: that is a boot strap pin

// The rgb+cct radio config, which is the one the FUT088/089/092 family uses.
static const uint16_t SYNCWORD_0 = 0x7236;
static const uint16_t SYNCWORD_3 = 0x1809;
static const uint8_t PREAMBLE = 0xAA;
static const uint8_t TRAILER = 0x05;

// The three channels this protocol hops across. A remote sends every press on all three,
// so one would do — but which one works is not knowable in advance: upstream's issue #847
// is someone capturing nothing until they changed it. Rotating removes the guess, at the
// cost of hearing only a third of the traffic.
static const uint8_t CHANNELS[] = {8, 39, 70};
static const uint32_t CHANNEL_DWELL_MS = 2000;

static RF24 rf24(PIN_CE, PIN_CSN);
static PL1167Rx radio(rf24);
static uint8_t syncword[MILIGHT_SYNCWORD_LEN];
static uint32_t packets = 0;
static uint32_t lastHop = 0;
static uint8_t channelIx = 0;

static void report(const uint8_t *raw, uint8_t length) {
  Serial.printf("\n[%lu] raw    :", (unsigned long)++packets);
  for (uint8_t i = 0; i < length; i++) {
    Serial.printf(" %02X", raw[i]);
  }
  Serial.println();

  if (length != V2_PACKET_LEN) {
    Serial.printf("      (not a %d-byte V2 packet, ignoring)\n", V2_PACKET_LEN);
    return;
  }

  uint8_t decoded[V2_PACKET_LEN];
  memcpy(decoded, raw, V2_PACKET_LEN);
  v2Decode(decoded);

  Serial.printf("      decoded:");
  for (uint8_t i = 0; i < V2_PACKET_LEN; i++) {
    Serial.printf(" %02X", decoded[i]);
  }
  Serial.println();

  Serial.printf("      protocol 0x%02X%s\n", decoded[V2_PROTOCOL],
                v2IsFut089(decoded) ? "  <-- FUT088/089/092 family" : "  (other remote)");
  Serial.printf("      DEVICE ID 0x%04X   group %u   command 0x%02X arg 0x%02X seq %u\n",
                v2DeviceId(decoded), decoded[V2_GROUP], decoded[V2_COMMAND],
                decoded[V2_ARGUMENT], decoded[V2_SEQUENCE]);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== pool-lights: MiLight sniffer (receive only) ==="));
  Serial.printf("build   : %s %s\n", __DATE__, __TIME__);
  Serial.printf("listen  : rotating %u/%u/%u every %lums\n",
                CHANNELS[0], CHANNELS[1], CHANNELS[2], (unsigned long)CHANNEL_DWELL_MS);

  SPI.begin();
  milightSyncword(SYNCWORD_0, SYNCWORD_3, PREAMBLE, TRAILER, syncword);

  if (!radio.begin(syncword, V2_PACKET_LEN)) {
    Serial.println(F("radio   : FAILED to start — run `make radio` first"));
    return;
  }
  Serial.println(F("radio   : listening. Press buttons on the remote.\n"));
}

void loop() {
  const uint32_t now = millis();
  if (elapsed(now, lastHop, CHANNEL_DWELL_MS)) {
    lastHop = now;
    channelIx = (uint8_t)((channelIx + 1) % (sizeof(CHANNELS) / sizeof(CHANNELS[0])));
    Serial.printf("... listening on channel %u (%lu packets so far)\n",
                  CHANNELS[channelIx], (unsigned long)packets);
  }

  uint8_t payload[V2_PACKET_LEN];
  const uint8_t length = radio.receive(CHANNELS[channelIx], payload, sizeof(payload));
  if (length > 0) {
    report(payload, length);
  }
}
