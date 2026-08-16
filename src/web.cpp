#include "web.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "log.h"
#include "page.h"

void WebUi::loop() {
  if (!_started) {
    if (WiFi.status() == WL_CONNECTED) {
      start();
    }
    return;
  }
  _server.handleClient();
}

void WebUi::start() {
  _server.on("/", HTTP_GET, [this]() { _server.send_P(200, "text/html", PAGE_HTML); });
  _server.on("/api/state", HTTP_GET, [this]() { handleState(); });
  _server.on("/api/set", HTTP_POST, [this]() { handleSet(); });
  _server.on("/log", HTTP_GET, [this]() { handleLog(); });
  _server.onNotFound([this]() { _server.send(404, "text/plain", "not found"); });
  _server.begin();

  // ArduinoOTA already started mDNS under the same hostname; advertising the web service
  // is what makes the board show up as a browsable device rather than just a pingable one.
  MDNS.addService("http", "tcp", 80);

  _started = true;
  logLine("web       : http://%s.local/  (http://%s/)", _hostname,
                WiFi.localIP().toString().c_str());
}

void WebUi::handleState() {
  const LightState &s = _control.state();

  // Hand-rolled rather than a JSON library: one object, fixed shape, and this avoids
  // pulling ArduinoJson in for 200 bytes.
  char body[288];
  snprintf(body, sizeof(body),
           "{\"on\":%s,\"night\":%s,\"brightness\":%u,\"mode\":\"%s\",\"hue\":%u,"
           "\"saturation\":%u,\"kelvin\":%u,\"effect\":%u,\"version\":%lu,"
           "\"host\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"heap\":%u}",
           s.on() ? "true" : "false", s.night() ? "true" : "false", s.brightness(),
           s.mode() == COLOUR_MODE_RGB ? "rgb" : "white", s.hue(), s.saturation(),
           s.kelvin(), s.effect(), (unsigned long)s.version(), _hostname,
           WiFi.localIP().toString().c_str(), WiFi.RSSI(),
           (unsigned long)(millis() / 1000), ESP.getFreeHeap());

  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "application/json", body);
}

// Streamed a line at a time: the buffer is 4 KB and assembling it into one response
// would need that much again from a heap with about 44 KB free.
void WebUi::handleLog() {
  const LogBuffer &log = logBuffer();
  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "text/plain", "");
  for (uint8_t i = 0; i < log.count(); i++) {
    _server.sendContent(log.line(i));
    _server.sendContent("\n");
  }
  _server.sendContent("");
}

void WebUi::handleSet() {
  // Every parameter is optional and applied if present, so the page can send one control
  // per request without needing an endpoint each.
  if (_server.hasArg("on")) {
    _control.turnOn();
  }
  if (_server.hasArg("off")) {
    _control.turnOff();
  }
  if (_server.hasArg("white")) {
    _control.setWhite();
  }
  if (_server.hasArg("hue")) {
    _control.setHue((uint8_t)_server.arg("hue").toInt());
  }
  if (_server.hasArg("brightness")) {
    _control.setBrightness((uint8_t)_server.arg("brightness").toInt());
  }
  if (_server.hasArg("sat")) {
    _control.setSaturationOrKelvin((uint8_t)_server.arg("sat").toInt());
  }
  if (_server.hasArg("effect")) {
    _control.setEffect((uint8_t)_server.arg("effect").toInt());
  }
  handleState();
}
