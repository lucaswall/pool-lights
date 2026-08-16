#include "net.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#include "timing.h"

static const uint32_t CONNECT_TIMEOUT_MS = 20000;

void Net::begin(const char *hostname, const char *ssid, const char *password,
                const char *otaPassword) {
  _hostname = hostname;
  _otaPassword = otaPassword;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);   // credentials come from secrets.h; rewriting flash every boot
                            // only wears it out
  WiFi.setAutoReconnect(true);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);

  Serial.printf("wifi      : connecting to %s\n", ssid);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && !elapsed(millis(), start, CONNECT_TIMEOUT_MS)) {
    delay(200);
  }

  // Not fatal: the SDK keeps retrying, so a slow AP resolves itself without a reboot.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("wifi      : not up yet (status %d), retrying in background\n",
                  WiFi.status());
  }
}

bool Net::connected() const { return WiFi.status() == WL_CONNECTED; }

void Net::loop() {
  const bool up = connected();

  if (up) {
    // Started here rather than in begin() so a board that joins late still becomes
    // reachable over the air without a reboot.
    if (!_otaReady) {
      ArduinoOTA.setHostname(_hostname);
      ArduinoOTA.setPassword(_otaPassword);
      ArduinoOTA.onStart([]() { Serial.println(F("ota       : start")); });
      ArduinoOTA.onEnd([]() { Serial.println(F("ota       : done, rebooting")); });
      ArduinoOTA.onError([](ota_error_t e) { Serial.printf("ota       : error %u\n", e); });
      ArduinoOTA.begin();
      _otaReady = true;
      Serial.printf("ota       : ready on %s at %s\n", _hostname,
                    WiFi.localIP().toString().c_str());
    }
    ArduinoOTA.handle();
  }

  if (up != _wasConnected) {
    _wasConnected = up;
    if (up) {
      Serial.printf("wifi      : up, ip %s rssi %d\n", WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
    } else {
      Serial.println(F("wifi      : lost"));
    }
  }
}
