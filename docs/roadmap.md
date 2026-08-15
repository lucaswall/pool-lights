# Roadmap — the ladder

Each rung is verified on real hardware and tagged before the next one starts. The ordering
is deliberate: every rung flushes out a class of failure that would otherwise be
misdiagnosed several rungs later.

| # | Rung | What it proves | What it adds |
|---|---|---|---|
| 1 | **Blink + banner** | Toolchain, upload, serial, and the correct board variant (the `pinmap` line) | `platformio.ini`, `src/main.cpp` |
| 2 | **Serial round-trip** | Host→device direction, and that the sketch is *running* rather than boot-looping | ~8 lines in `main.cpp` |
| 3 | **WiFi connect** | Credentials and DHCP — but the real point is that the 3.3 V rail survives radio TX current bursts. Same power-integrity failure that kills a PA+LNA radio at rung 6, surfaced three rungs early and for free | `include/secrets.h.example`, gitignored `include/secrets.h` |
| 4 | **OTA** | Iterating without unplugging, *before* jumper wires cover the USB port. Caveat: you can only OTA a board whose running sketch handles OTA — one bad flash and you are back on USB | an OTA build env |
| 5 | **MQTT** | TCP stack, broker auth, and the Home-Assistant-facing half of the bridge, all with zero RF involved | an MQTT client library; `src/net.*`, `src/mqtt.*` |
| 6 | **SPI / radio self-test** | Wiring, SPI clock integrity, a genuine `+` part, and the power rail under RF load. Standalone sketch, before any protocol code | RF24 library; `tools/serial_log.py` |
| 7 | **MiLight bridge** | The destination: sniff the remote, emulate it, expose the light to Home Assistant | upstream firmware as a sibling clone — see `docs/milight-notes.md` |

Rungs 1–6 are ordinary ESP8266 work and stand on their own. Rung 7 is where this project
becomes specific.

## Why a standalone radio self-test (rung 6)

The upstream milight firmware never checks whether the radio answered — it discards the
return value of `begin()` and never calls `isChipConnected()`. An unwired radio therefore
boots, joins WiFi, connects to MQTT and serves its web UI looking perfectly healthy, while
no packet ever leaves the board. "It's online" is not evidence the radio exists. Prove the
radio separately, once, and that whole class of confusion disappears.
