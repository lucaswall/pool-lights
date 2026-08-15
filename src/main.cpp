// Rung 1 — hello world.
//
// Proves the whole toolchain in one shot: the build works, the upload works, the serial
// link works, and — via the pinmap line — that the correct board variant was compiled.
//
// Wemos D1 R1 (board = d1, esp8266 core variant "d1"):
//   LED_BUILTIN == GPIO2 (silkscreen D9), the LED on the ESP-12 module.
//   ACTIVE LOW: digitalWrite(LED_BUILTIN, LOW) turns it ON.
//   A second board LED sits on GPIO14 (the Uno-style "L" by the D13 pad) and is active
//   high. Never build on GPIO14: it is HSPI SCK and the NRF24 needs it later.

#include <Arduino.h>

#include "timing.h"

static const uint32_t BLINK_MS = 500;
static uint32_t lastToggle = 0;
static uint32_t tick = 0;
static bool ledOn = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);            // active low -> start off

  Serial.begin(115200);
  delay(200);                                 // let the CH340 side settle
  Serial.println();
  Serial.println(F("=== pool-lights: hello ==="));
  Serial.printf("core      : %s\n",       ESP.getCoreVersion().c_str());
  Serial.printf("sdk       : %s\n",       ESP.getSdkVersion());
  Serial.printf("chip id   : %06x\n",     ESP.getChipId());
  Serial.printf("cpu       : %u MHz\n",   ESP.getCpuFreqMHz());
  Serial.printf("flash real: %u bytes\n", ESP.getFlashChipRealSize());  // what the chip reports
  Serial.printf("flash cfg : %u bytes\n", ESP.getFlashChipSize());      // what we built for
  Serial.printf("sketch max: %u bytes\n", ESP.getFreeSketchSpace());
  Serial.printf("heap      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("reset     : %s\n",       ESP.getResetReason().c_str());

  // The variant check. On a real D1 R1 this MUST print:
  //   pinmap    : D2=16 D4=4 D8=0 D10=15 LED_BUILTIN=2
  // If you see D2=4 D4=2 D8=15 you built d1_mini and every pin number in your code is wrong.
  Serial.printf("pinmap    : D2=%u D4=%u D8=%u D10=%u LED_BUILTIN=%u\n",
                D2, D4, D8, D10, LED_BUILTIN);
}

void loop() {
  const uint32_t now = millis();
  if (elapsed(now, lastToggle, BLINK_MS)) {
    lastToggle = now;
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);   // LOW = on
    if (ledOn) {
      Serial.printf("tick %lu  up=%lus  heap=%u\n",
                    (unsigned long)++tick,
                    (unsigned long)(now / 1000),
                    ESP.getFreeHeap());
    }
  }
}
