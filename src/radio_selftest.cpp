// Standalone radio self-test, built by `make radio`.
//
// Upstream's firmware discards begin()'s return and never calls isChipConnected(), so an
// unwired radio looks healthy on WiFi and MQTT and the wiring fault surfaces days later
// as a protocol mystery. Every failure reported here is wiring or power, never software.

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "radiodiag.h"

// CE miswiring is undetectable by any register test — check it by eye. CSN avoids the
// usual GPIO15: it idles high, and GPIO15 high at reset stops the board booting.
static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;

// RF24 defaults to 10 MHz, which dupont wire often cannot carry. Falling back to 4 MHz
// tells us "marginal wiring" rather than "no radio".
static const uint32_t SPI_HZ[] = {10000000, 4000000};

// Neither 0x00 nor 0xff, so a stuck or floating bus cannot imitate a pass.
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

  // Only meaningful once the bus is proven: a non-plus part cannot do 250 kbps.
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

  uint32_t passedAt = 0;
  for (uint8_t i = 0; i < sizeof(SPI_HZ) / sizeof(SPI_HZ[0]) && passedAt == 0; i++) {
    if (probe(SPI_HZ[i])) {
      passedAt = SPI_HZ[i];
    }
  }

  Serial.println();
  if (passedAt == SPI_HZ[0]) {
    Serial.println(F("RESULT: PASS — radio is wired and answering at the full clock"));
  } else if (passedAt > 0) {
    // The firmware and the sniffer both run at the default 10 MHz, so a pass only at the
    // fallback speed is a warning, not a clean bill of health.
    Serial.printf("RESULT: MARGINAL — passed only at %lu Hz, but the firmware runs at "
                  "%lu Hz. Shorten the leads.\n",
                  (unsigned long)passedAt, (unsigned long)SPI_HZ[0]);
  } else {
    Serial.println(F("RESULT: FAIL — see the fault above. Wiring or power, not code."));
  }
}

void loop() {
  delay(1000);
}
