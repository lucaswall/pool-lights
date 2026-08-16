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
#include "timing.h"
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

// Reporting heap only on new lows keeps a healthy board quiet while a leak shows up as a
// steady descent.
static const uint32_t HEAP_REPORT_STEP = 1024;
static uint32_t heapLowWater = 0;

// A heartbeat, so an idle log is visibly idle rather than ambiguously stalled, and an
// hourly line that is worth reading back through days of history.
static const uint32_t HEALTH_MS = 5UL * 60 * 1000;
static const uint32_t STATUS_MS = 60UL * 60 * 1000;
static uint32_t lastHealth = 0;
static uint32_t lastStatus = 0;

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

  heapLowWater = ESP.getFreeHeap();
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

  const uint32_t now = millis();
  if (elapsed(now, lastHealth, HEALTH_MS)) {
    lastHealth = now;
    logLine("health    : heap %lu rssi %d", (unsigned long)ESP.getFreeHeap(), net.rssi());
  }
  if (elapsed(now, lastStatus, STATUS_MS)) {
    lastStatus = now;
    const uint32_t up = now / 1000;
    logLine("status    : up %luh%02lum heap %lu low %lu rssi %d wifi %s mqtt %s light %s",
            (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60),
            (unsigned long)ESP.getFreeHeap(), (unsigned long)heapLowWater, net.rssi(),
            net.connected() ? "up" : "down", mqtt.connected() ? "up" : "down",
            control.state().on() ? "on" : "off");
  }

  const uint32_t heap = ESP.getFreeHeap();
  if (heap + HEAP_REPORT_STEP < heapLowWater) {
    heapLowWater = heap;
    logLine("heap      : new low %lu bytes", (unsigned long)heap);
  }

  if (control.state().version() != reportedVersion) {
    reportedVersion = control.state().version();
    reportState();
  }
}
