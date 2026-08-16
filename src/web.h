#pragma once

#include <ESP8266WebServer.h>

#include "control.h"
#include "net.h"

// Debug UI: one page, a state endpoint the browser polls, and one command endpoint.
//
// Deliberately the synchronous server bundled with the core rather than an async one.
// Free heap here is around 48 KB, the state payload is under 200 bytes, and the radio
// blocks for a few hundred milliseconds on every transmit — none of which argues for a
// second TCP stack and a WebSocket. A request arriving mid-burst simply waits.
class WebUi {
 public:
  WebUi(Control &control, Net &net, const char *hostname)
      : _server(80), _control(control), _net(net), _hostname(hostname) {}

  // Starts itself once WiFi is up, the same way OTA does, so main does not have to
  // sequence them.
  void loop();

 private:
  void start();
  void handleState();
  void handleSet();
  uint8_t argByte(const char *name, long max);
  void handleLog();
  void handleErrors();
  void handleStatus();

  bool _started = false;

  ESP8266WebServer _server;
  Control &_control;
  Net &_net;
  const char *_hostname;
};
