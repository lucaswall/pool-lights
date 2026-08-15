// Rungs 1-4: blink + banner, serial, WiFi, OTA.
//
// Wemos D1 R1 (board = d1, esp8266 core variant "d1"):
//   LED_BUILTIN == GPIO2 (silkscreen D9), ACTIVE LOW — digitalWrite(LOW) turns it ON.
//   A second board LED sits on GPIO14 and is active high. Never build on GPIO14: it is
//   HSPI SCK and the NRF24 needs it later.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#include "netid.h"
#include "secrets.h"
#include "timing.h"

static const uint32_t BLINK_MS = 500;
static const uint32_t WIFI_TIMEOUT_MS = 20000;

static char host[NETID_LEN];
static uint32_t lastToggle = 0;
static uint32_t tick = 0;
static bool ledOn = false;
static bool otaReady = false;
static bool wasConnected = false;

static void banner() {
  Serial.println();
  Serial.println(F("=== pool-lights: hello ==="));
  // Which firmware is actually on the board? After an OTA there is no other way to tell.
  Serial.printf("build     : %s %s\n",    __DATE__, __TIME__);
  Serial.printf("core      : %s\n",       ESP.getCoreVersion().c_str());
  Serial.printf("sdk       : %s\n",       ESP.getSdkVersion());
  Serial.printf("chip id   : %06x\n",     ESP.getChipId());
  Serial.printf("host      : %s\n",       host);
  Serial.printf("cpu       : %u MHz\n",   ESP.getCpuFreqMHz());
  Serial.printf("flash real: %u bytes\n", ESP.getFlashChipRealSize());
  Serial.printf("flash cfg : %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("sketch max: %u bytes\n", ESP.getFreeSketchSpace());
  Serial.printf("heap      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("reset     : %s\n",       ESP.getResetReason().c_str());

  // On a real D1 R1 this MUST print D2=16 D4=4 D8=0 D10=15. D2=4 D4=2 D8=15 means
  // d1_mini was built and every pin number in this file is wrong.
  Serial.printf("pinmap    : D2=%u D4=%u D8=%u D10=%u LED_BUILTIN=%u\n",
                D2, D4, D8, D10, LED_BUILTIN);
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);      // credentials come from secrets.h; writing them to flash on
                               // every boot only wears it out
  WiFi.setAutoReconnect(true);
  WiFi.hostname(host);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("wifi      : connecting to %s\n", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && !elapsed(millis(), start, WIFI_TIMEOUT_MS)) {
    delay(200);
  }

  // Not fatal: the SDK keeps retrying in the background, so a slow AP or a late DHCP
  // lease resolves itself without a reboot.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("wifi      : not up yet (status %d), retrying in background\n",
                  WiFi.status());
  }
}

static void startOta() {
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println(F("ota       : start")); });
  ArduinoOTA.onEnd([]()   { Serial.println(F("ota       : done, rebooting")); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("ota       : error %u\n", e); });
  ArduinoOTA.begin();
  otaReady = true;
  Serial.printf("ota       : ready on %s at %s\n", host, WiFi.localIP().toString().c_str());
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);            // active low -> start off

  Serial.begin(115200);
  delay(200);                                 // let the CH340 side settle
  deviceName(ESP.getChipId(), host, sizeof(host));
  banner();
  connectWifi();
}

void loop() {
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    // Started here rather than in setup() so a board that joins late still gets OTA
    // without a reboot.
    if (!otaReady) {
      startOta();
    }
    ArduinoOTA.handle();
  }

  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) {
      Serial.printf("wifi      : up, ip %s rssi %d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println(F("wifi      : lost"));
    }
  }

  const uint32_t now = millis();
  if (elapsed(now, lastToggle, BLINK_MS)) {
    lastToggle = now;
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);   // LOW = on
    if (ledOn) {
      Serial.printf("tick %lu  up=%lus  heap=%u  wifi=%d\n",
                    (unsigned long)++tick,
                    (unsigned long)(now / 1000),
                    ESP.getFreeHeap(),
                    WiFi.status());
    }
  }
}
