// pool-lights — MiLight bridge.
//
// Construction and the loop, nothing else. Behaviour lives in Control (intent and state),
// RadioLink (the half-duplex radio), Net (WiFi and OTA) and WebUi (the debug interface).
//
// Wemos D1 R1: LED_BUILTIN is GPIO2, active low. Never build on GPIO14 — it is HSPI SCK
// and the radio needs it.

#include <Arduino.h>

#include "log.h"

#include "control.h"
#include "ha_mqtt.h"
#include "net.h"
#include "radio_link.h"
#include "secrets.h"
#include "web.h"

// CE is a plain strobe outside SPI, so no register test can catch it miswired. CSN avoids
// GPIO15, which must be low at reset and would stop the board booting.
static const uint8_t PIN_CE = 4;
static const uint8_t PIN_CSN = 5;

// One bridge, so a plain name: it is what shows up in the router's client list for the
// DHCP reservation, and what resolves as pool-lights.local.
static const char HOST[] = "pool-lights";
static Net net;
static RadioLink radio(PIN_CE, PIN_CSN);
static Control control(radio, MILIGHT_DEVICE_ID, MILIGHT_GROUP);
static WebUi web(control, HOST);
static HaMqtt mqtt(control, HOST);
static uint32_t reportedVersion = 0;

static void banner() {
  Serial.println();
  logLine("=== pool-lights ===");
  logLine("build     : %s %s", __DATE__, __TIME__);
  logLine("host      : %s", HOST);
  logLine("heap      : %u bytes", ESP.getFreeHeap());
  logLine("reset     : %s", ESP.getResetReason().c_str());
  logLine("pinmap    : D2=%u D4=%u D8=%u D10=%u LED_BUILTIN=%u",
                D2, D4, D8, D10, LED_BUILTIN);
}

static void reportState() {
  const LightState &s = control.state();
  logLine("state     : %s%s bright %u %s hue %u sat %u kelvin %u effect %u",
                s.on() ? "ON" : "OFF", s.night() ? " (night)" : "", s.brightness(),
                s.mode() == COLOUR_MODE_RGB ? "rgb" : "white", s.hue(), s.saturation(),
                s.kelvin(), s.effect());
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);   // active low, so start off

  Serial.begin(115200);
  delay(200);
  banner();

  if (!radio.begin()) {
    logLine("radio     : FAILED to start — run `make radio`");
  } else {
    logLine("radio     : listening");
  }

  net.begin(HOST, WIFI_SSID, WIFI_PASSWORD, OTA_PASSWORD);
}

void loop() {
  net.loop();   // before web: this is what starts mDNS, which web then advertises on
  web.loop();
  mqtt.loop();
  radio.loop();

  Packet packet;
  if (radio.take(&packet)) {
    control.onReceived(packet);
    logLine("heard     : %s%s (0x%02X arg 0x%02X) from 0x%04X group %u",
                  packet.held ? "HELD " : "",
                  v2CommandName(packet.command, packet.argument), packet.command,
                  packet.argument, packet.deviceId, packet.group);
  }

  if (control.state().version() != reportedVersion) {
    reportedVersion = control.state().version();
    reportState();
  }
}
