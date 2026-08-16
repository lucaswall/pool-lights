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
// Seeded at construction from the clock, so a reboot does not reopen with the same
// packet the previous boot sent first.
static Control control(radio, MILIGHT_DEVICE_ID, MILIGHT_GROUP,
                       (uint8_t)(micros() & 0xFF));
static WebUi web(control, net, HOST);
static HaMqtt mqtt(control, HOST);
static uint32_t reportedVersion = 0;

// A radio that fails to start is not recoverable by hand any more: the board is sealed.
// Retry it from the loop instead of logging once and running blind forever.
static const uint32_t RADIO_RETRY_MS = 30000;
static bool radioReady = false;
static uint32_t lastRadioTry = 0;

// Reporting heap only on new lows keeps a healthy board quiet while a leak shows up as a
// steady descent.
static const uint32_t HEAP_REPORT_STEP = 1024;
static uint32_t heapLowWater = 0;

// A heartbeat, so an idle log reads as idle rather than as a stalled page. Slots are
// fixed width, so a terse line costs exactly what a full one does — there is nothing to
// be saved by reporting less. Uptime is deliberately absent: every line already carries
// it as a timestamp, and repeating it here would stop identical readings collapsing.
static const uint32_t HEALTH_MS = 5UL * 60 * 1000;
static uint32_t lastHealth = 0;

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

  radioReady = radio.begin();
  if (!radioReady) {
    logError("radio     : FAILED to start, will retry — run `make radio`");
  } else {
    logLine("radio     : listening");
  }

  heapLowWater = ESP.getFreeHeap();
  net.begin(HOST, WIFI_SSID, WIFI_PASSWORD, OTA_PASSWORD);
}

void loop() {
  const uint32_t now = millis();

  net.loop();   // before web: this is what starts mDNS, which web then advertises on
  web.loop();
  mqtt.loop();

  if (radioReady) {
    radio.loop();
  } else if (elapsed(millis(), lastRadioTry, RADIO_RETRY_MS)) {
    lastRadioTry = millis();
    radioReady = radio.begin();
    if (radioReady) {
      logLine("radio     : recovered, listening");
    }
  }

  control.tick(now);

  Packet packet;
  if (radio.take(&packet)) {
    control.onReceived(packet);
    logLine("heard     : %s%s (0x%02X arg 0x%02X) from 0x%04X group %u",
                  packet.held ? "HELD " : "",
                  v2CommandName(packet.command, packet.argument), packet.command,
                  packet.argument, packet.deviceId, packet.group);
  }

  if (elapsed(now, lastHealth, HEALTH_MS)) {
    lastHealth = now;
    logLine("health    : heap %lu low %lu rssi %d wifi %s mqtt %s light %s faults %u",
            (unsigned long)ESP.getFreeHeap(), (unsigned long)heapLowWater, net.rssi(),
            net.connected() ? "up" : "down", mqtt.connected() ? "up" : "down",
            control.state().on() ? "on" : "off", errorBuffer().count());
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
