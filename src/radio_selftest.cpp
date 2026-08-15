// Rung 6 — standalone radio self-test. Built by `make radio`, not by the normal firmware.
//
// It exists because the upstream milight firmware discards RF24::begin()'s return value
// and never calls isChipConnected(): an unwired radio still boots, joins WiFi and serves
// its web UI looking healthy, so a wiring fault gets diagnosed as a protocol problem days
// later. Prove the radio separately, once.
//
// Every failure this reports is wiring or power. None of them are software.

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "radiodiag.h"

// CE is a plain strobe outside SPI, so no register test can detect it miswired — check it
// by eye. CSN is on GPIO5 rather than the usual GPIO15, which is a boot strap pin: CSN
// idles high and would stop the board booting at all.
static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;

// RF24 defaults to 10 MHz, which dupont wire often cannot carry. Falling back to 4 MHz
// tells us "marginal wiring" rather than "no radio".
static const uint32_t SPI_HZ[] = {10000000, 4000000};

// Arbitrary but distinctive: 0x4c is not 0x00 or 0xff, so a stuck or floating bus cannot
// imitate a successful readback. Channel is 7 bits, so the top bit reads back clear.
static const uint8_t TEST_CHANNEL = 0x4c;

static bool probe(uint32_t hz) {
  RF24 radio(PIN_CE, PIN_CSN, hz);

  Serial.printf("\n--- probing at %lu Hz ---\n", (unsigned long)hz);

  const bool began = radio.begin();
  const bool connected = radio.isChipConnected();
  Serial.printf("begin()          : %s\n", began ? "true" : "false");
  Serial.printf("isChipConnected(): %s\n", connected ? "true" : "false");

  radio.setChannel(TEST_CHANNEL);
  const uint8_t readBack = radio.getChannel();
  const RadioFault fault = diagnose(TEST_CHANNEL, readBack);
  Serial.printf("channel wrote    : 0x%02x\n", TEST_CHANNEL);
  Serial.printf("channel read     : 0x%02x  -> %s\n", readBack, faultName(fault));

  if (fault != RADIO_OK) {
    return false;
  }

  // Only meaningful once the bus is proven; a fake or non-plus part cannot do the
  // 250 kbps rate the MiLight protocol needs.
  const bool plus = radio.isPVariant();
  Serial.printf("isPVariant()     : %s%s\n", plus ? "true" : "false",
                plus ? "" : "  <-- not a genuine NRF24L01+");

  return began && connected && plus;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== pool-lights: radio self-test ==="));
  Serial.printf("build   : %s %s\n", __DATE__, __TIME__);
  Serial.printf("wiring  : CE=GPIO%u CSN=GPIO%u SCK=GPIO14 MOSI=GPIO13 MISO=GPIO12\n",
                PIN_CE, PIN_CSN);

  SPI.begin();

  bool passed = false;
  for (uint8_t i = 0; i < sizeof(SPI_HZ) / sizeof(SPI_HZ[0]) && !passed; i++) {
    passed = probe(SPI_HZ[i]);
  }

  Serial.println();
  Serial.println(passed ? F("RESULT: PASS — radio is wired and answering")
                        : F("RESULT: FAIL — see the fault above. Wiring or power, not code."));
}

void loop() {
  delay(1000);
}
