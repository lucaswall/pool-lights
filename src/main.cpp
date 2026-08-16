// pool-lights — MiLight bridge.
//
// Construction and the loop, nothing else. The behaviour lives in Control (intent and
// state), RadioLink (the half-duplex radio) and Net (WiFi and OTA).
//
// Wemos D1 R1: LED_BUILTIN is GPIO2, active low. Never build on GPIO14 — it is HSPI SCK
// and the radio needs it.

#include <Arduino.h>

#include "control.h"
#include "netid.h"
#include "radio_link.h"
#include "secrets.h"
#include "net.h"

// CE is a plain strobe outside SPI, so no register test can catch it miswired. CSN avoids
// GPIO15, which must be low at reset and would stop the board booting.
static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;

static char host[NETID_LEN];
static Net net;
static RadioLink radio(PIN_CE, PIN_CSN);
static Control control(radio, MILIGHT_DEVICE_ID, MILIGHT_GROUP);

static void banner() {
  Serial.println();
  Serial.println(F("=== pool-lights ==="));
  Serial.printf("build     : %s %s\n", __DATE__, __TIME__);
  Serial.printf("host      : %s\n", host);
  Serial.printf("heap      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("reset     : %s\n", ESP.getResetReason().c_str());
  Serial.printf("pinmap    : D2=%u D4=%u D8=%u D10=%u LED_BUILTIN=%u\n",
                D2, D4, D8, D10, LED_BUILTIN);
}

static void reportState() {
  const LightState &s = control.state();
  Serial.printf("state     : %s%s bright %u %s hue %u sat %u kelvin %u effect %u\n",
                s.on() ? "ON" : "OFF", s.night() ? " (night)" : "", s.brightness(),
                s.mode() == COLOUR_MODE_RGB ? "rgb" : "white", s.hue(), s.saturation(),
                s.kelvin(), s.effect());
}

// A keyboard stands in for the web UI until there is one. It exercises the same Control
// interface the UI will use, so the send path is covered rather than waiting unused.
static void console(char key) {
  switch (key) {
    case 'o': control.turnOn(); break;
    case 'f': control.turnOff(); break;
    case 'w': control.setWhite(); break;
    case 'r': control.setHue(0x00); break;
    case 'g': control.setHue(0x55); break;
    case 'b': control.setHue(0xAB); break;
    case '1': control.setBrightness(10); break;
    case '9': control.setBrightness(100); break;
    case 's': reportState(); return;
    case '?':
      Serial.println(F("keys: o=on f=off w=white r/g/b=colour 1/9=brightness s=state"));
      return;
    default: return;
  }
  Serial.printf("sent      : %c\n", key);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);   // active low, so start off

  Serial.begin(115200);
  delay(200);
  deviceName(ESP.getChipId(), host, sizeof(host));
  banner();

  if (!radio.begin()) {
    Serial.println(F("radio     : FAILED to start — run `make radio`"));
  } else {
    Serial.println(F("radio     : listening"));
  }

  net.begin(host, WIFI_SSID, WIFI_PASSWORD, OTA_PASSWORD);
  Serial.println(F("ready. '?' for keys."));
}

void loop() {
  net.loop();
  radio.loop();

  Packet packet;
  if (radio.take(&packet)) {
    control.onReceived(packet);
    Serial.printf("heard     : %s%s (0x%02X arg 0x%02X) from 0x%04X group %u\n",
                  packet.held ? "HELD " : "",
                  v2CommandName(packet.command, packet.argument), packet.command,
                  packet.argument, packet.deviceId, packet.group);
  }

  if (control.state().dirty()) {
    control.state().clearDirty();
    reportState();
  }

  if (Serial.available()) {
    console((char)Serial.read());
  }
}
