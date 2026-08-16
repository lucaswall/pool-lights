#include "ha_mqtt.h"

#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "log.h"
#include "timing.h"
#include "secrets.h"
#include "units.h"

// Topics. MQTT_PREFIX keeps us inside our own tree on a broker shared with other
// integrations.
#define T_STATE MQTT_PREFIX "light/state"
#define T_SET MQTT_PREFIX "light/set"
#define T_AVAIL MQTT_PREFIX "status"
#define T_FASTER MQTT_PREFIX "effect/faster"
#define T_SLOWER MQTT_PREFIX "effect/slower"

// Backed off to a cap: a broker that is simply switched off should not have us blocking
// on a connect attempt every few seconds. Each attempt costs the socket timeout below,
// and while it blocks, ArduinoOTA and the radio do not run.
static const uint32_t RETRY_MIN_MS = 5000;
static const uint32_t RETRY_MAX_MS = 60000;

// PubSubClient defaults to 15 s and WiFiClient to 5 s. Fifteen seconds without servicing
// ArduinoOTA is longer than espota waits for an invitation, which on a sealed board means
// a broker outage could cost the ability to reflash. Two seconds is ample on a LAN.
static const uint16_t SOCKET_TIMEOUT_S = 2;
static const uint32_t CLIENT_TIMEOUT_MS = 2000;

void HaMqtt::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!_mqtt.connected()) {
    const uint32_t now = millis();
    if (_attempted && !elapsed(now, _lastAttempt, _retryMs)) {
      return;
    }
    _attempted = true;
    const bool ok = connect();
    // Stamped after the attempt, not before: a connect that burns its whole timeout would
    // otherwise return with the retry window already expired and spin.
    _lastAttempt = millis();
    if (!ok) {
      _retryMs = _retryMs >= RETRY_MAX_MS ? RETRY_MAX_MS : _retryMs * 2;
      return;
    }
    _retryMs = RETRY_MIN_MS;
  }

  _mqtt.loop();

  // Publish on any change, whoever caused it — a command from Home Assistant or a press
  // on the physical remote. Same version counter the web UI polls.
  if (_control.state().version() != _publishedVersion) {
    _publishedVersion = _control.state().version();
    publishState();
  }
}

bool HaMqtt::connect() {
  _mqtt.setServer(MQTT_HOST, MQTT_PORT);
  _mqtt.setBufferSize(1024);   // the discovery payload does not fit the 256-byte default
  _mqtt.setSocketTimeout(SOCKET_TIMEOUT_S);
  _wifi.setTimeout(CLIENT_TIMEOUT_MS);
  _mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    onMessage(topic, payload, length);
  });

  // The last will is the whole availability story: a crashed bridge looks exactly like an
  // idle one, so the broker has to be the one to say we are gone.
  if (!_mqtt.connect(_hostname, MQTT_USER, MQTT_PASSWORD, T_AVAIL, 0, true, "offline")) {
    logError("mqtt      : connect failed, state %d", _mqtt.state());
    return false;
  }

  _mqtt.publish(T_AVAIL, "online", true);
  _mqtt.subscribe(T_SET);
  _mqtt.subscribe(T_FASTER);
  _mqtt.subscribe(T_SLOWER);
  publishDiscovery();
  publishState();

  logLine("mqtt      : connected to %s as %s", MQTT_HOST, MQTT_USER);
  return true;
}

void HaMqtt::publishDiscovery() {
  // Retained, so Home Assistant recreates the entities after its own restart without
  // waiting for us to reconnect.
  JsonDocument doc;
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = _hostname;
  device["name"] = "Pool Lights";
  device["model"] = "MiBoxer FUT088 bridge";
  device["manufacturer"] = "pool-lights";
  device["sw_version"] = __DATE__;
  char url[48];
  snprintf(url, sizeof(url), "http://%s/", WiFi.localIP().toString().c_str());
  device["configuration_url"] = url;

  doc["availability_topic"] = T_AVAIL;
  doc["schema"] = "json";
  doc["state_topic"] = T_STATE;
  doc["command_topic"] = T_SET;
  doc["name"] = nullptr;   // null means "use the device name" for the primary entity
  doc["unique_id"] = "pool_lights_light";
  doc["brightness"] = true;
  doc["brightness_scale"] = 100;   // so Home Assistant speaks the protocol's own units
  doc["supported_color_modes"][0] = "rgb";
  doc["supported_color_modes"][1] = "color_temp";
  doc["min_mireds"] = HA_MIRED_MIN;
  doc["max_mireds"] = HA_MIRED_MAX;
  doc["effect"] = true;
  for (uint8_t i = 0; i < 5; i++) {
    doc["effect_list"][i] = String(i);
  }

  char payload[1024];
  serializeJson(doc, payload, sizeof(payload));
  if (!_mqtt.publish("homeassistant/light/pool_lights/config", payload, true)) {
    logError("mqtt      : light discovery rejected (%u bytes)", (unsigned)strlen(payload));
  }

  // Two buttons. They are momentary — no state to report, so no state topic.
  const char *ids[] = {"faster", "slower"};
  const char *names[] = {"Effect faster", "Effect slower"};
  const char *topics[] = {T_FASTER, T_SLOWER};
  for (uint8_t i = 0; i < 2; i++) {
    JsonDocument btn;
    btn["device"]["identifiers"][0] = _hostname;
    btn["availability_topic"] = T_AVAIL;
    btn["command_topic"] = topics[i];
    btn["name"] = names[i];
    char uid[40];
    snprintf(uid, sizeof(uid), "pool_lights_%s", ids[i]);
    btn["unique_id"] = uid;

    char cfg[64];
    snprintf(cfg, sizeof(cfg), HA_DISCOVERY_PREFIX "button/pool_lights_%s/config",
             ids[i]);
    serializeJson(btn, payload, sizeof(payload));
    if (!_mqtt.publish(cfg, payload, true)) {
      logError("mqtt      : button discovery rejected (%s)", ids[i]);
    }
  }
}

void HaMqtt::publishState() {
  const LightState &s = _control.state();

  JsonDocument doc;
  doc["state"] = s.on() ? "ON" : "OFF";
  // Night mode is a glow below anything the brightness bar can select, so reporting the
  // last commanded percentage would show a bright light that is nearly dark.
  doc["brightness"] = s.night() ? 1 : s.brightness();
  doc["effect"] = String(s.effect());

  if (s.mode() == COLOUR_MODE_WHITE) {
    doc["color_mode"] = "color_temp";
    doc["color_temp"] = protocolToMireds(s.kelvin());
  } else {
    doc["color_mode"] = "rgb";
    uint8_t r, g, b;
    hueToRgb(s.hue(), &r, &g, &b);
    doc["color"]["r"] = r;
    doc["color"]["g"] = g;
    doc["color"]["b"] = b;
  }

  char payload[256];
  serializeJson(doc, payload, sizeof(payload));
  if (!_mqtt.publish(T_STATE, payload, true)) {
    logError("mqtt      : state publish rejected (%u bytes)", (unsigned)strlen(payload));
  }
}

void HaMqtt::onMessage(const char *topic, const uint8_t *payload, unsigned int length) {
  if (strcmp(topic, T_FASTER) == 0) {
    _control.effectFaster();
    return;
  }
  if (strcmp(topic, T_SLOWER) == 0) {
    _control.effectSlower();
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    logError("mqtt      : could not parse a command on %s", topic);
    return;
  }

  // Order matters, and it depends on whether the light is already on. The receiver
  // discards colour and brightness sent to a light that is off, and then ON restores
  // whatever it last held — so "turn on blue" from off used to come up in the previous
  // colour, and only a second identical command took effect.
  //
  // Waking it first costs a brief flash of the old colour. Sending attributes first when
  // it is already on avoids that flash, and is safe because they land.
  const bool wantOn = doc["state"].is<const char *>() && strcmp(doc["state"], "ON") == 0;
  const bool wokeIt = wantOn && !_control.state().on();
  if (wokeIt) {
    _control.turnOn();
  }

  if (doc["color"].is<JsonObject>()) {
    const uint8_t r = doc["color"]["r"], g = doc["color"]["g"], b = doc["color"]["b"];
    // Every grey has hue 0, which is red. A near-white request belongs on the white
    // channel — which is what the W key on the remote selects — not on a washed-out hue.
    if (rgbSaturation(r, g, b) < WHITE_SATURATION_THRESHOLD) {
      _control.setWhite();
    } else {
      _control.setHue(rgbToHue(r, g, b));
    }
  }
  if (doc["color_temp"].is<uint16_t>()) {
    _control.setWhite();
    _control.setSaturationOrKelvin(miredsToProtocol(doc["color_temp"]));
  }
  if (doc["brightness"].is<uint8_t>()) {
    _control.setBrightness(doc["brightness"]);
  }
  if (doc["effect"].is<const char *>()) {
    _control.setEffect((uint8_t)atoi(doc["effect"]));
  }
  if (doc["state"].is<const char *>()) {
    if (!wantOn) {
      _control.turnOff();
    } else if (!wokeIt) {
      _control.turnOn();   // already on: the attributes landed, confirm the power state
    }
  }
}
