#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>

#include "control.h"

// The only place that knows Home Assistant exists. Control speaks intent and protocol
// units; everything HA-shaped — discovery payloads, topic names, mireds, RGB triples —
// stops here.
//
// One device carrying three entities: the light, and a button each for effect speed. The
// speed keys are momentary actions with no state, so they are buttons rather than
// anything the light entity could express.
class HaMqtt {
 public:
  HaMqtt(Control &control, const char *hostname) : _mqtt(_wifi), _control(control),
                                                   _hostname(hostname) {}

  void loop();

 private:
  bool connect();
  void publishDiscovery();
  void publishState();
  void onMessage(const char *topic, const uint8_t *payload, unsigned int length);

  WiFiClient _wifi;
  PubSubClient _mqtt;
  Control &_control;
  const char *_hostname;

  uint32_t _publishedVersion = 0;
  uint32_t _lastAttempt = 0;
  bool _everConnected = false;
};
