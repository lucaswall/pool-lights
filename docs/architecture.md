# Architecture

The shape of the firmware from the MQTT rung onward. `docs/code-standards.md` still
governs: minimal, no dead things, and anything with a definable input and output is tested
on the desktop.

## The split that drives everything

`pio test` does not build `src/`, so testable code lives in headers under `include/`. That
is not a limitation to work around — it is the design constraint. **Everything that is not
a peripheral belongs in `include/` and is tested natively.**

```
include/            pure, no Arduino headers, unit tested
  milight_wire.h    PL1167 framing helpers          (exists)
  v2rf.h            V2 packet codec                 (exists)
  packet.h          typed packet + decode result
  encoder.h         Encoder: device id, group, sequence
  state.h           LightState: what the light is doing
  units.h           Home Assistant <-> protocol conversions
lib/PL1167/         radio PHY                       (exists)
src/                peripherals and wiring
  radio_link.*      half-duplex arbitration
  net.*             WiFi + OTA
  ha_mqtt.*         discovery and topic translation
  control.*         the facade
  main.cpp          construction and loop, nothing else
```

## Components

**`Packet`** — a decoded command as a value: device id, group, command, argument,
sequence, held. `decodePacket(raw, &out)` returns false on a foreign protocol or a bad
length. Decoding is stateless, so it is a function returning a value, not a class with no
members.

**`Encoder`** — holds device id, group and the rolling sequence number, which is real
state. `encode(command, arg, out)` produces a ready-to-send packet.

**`LightState`** — on/off, brightness, colour mode, hue, saturation, kelvin, effect.
Updated by exactly one method, `apply(const Packet&)`, so a command we sent and a press we
overheard from the physical remote travel the same code path. Tracks a dirty flag so the
MQTT layer knows when to publish.

**`units.h`** — Home Assistant speaks RGB triples and colour temperature; the protocol
speaks a single hue byte and a 0–100 scale. Rounding, clamping and the hue wrap at 0/360
are where silent bugs live, so the conversions are pure functions with tests.

**`RadioLink`** — owns the PL1167 and arbitrates the radio, which is half-duplex. Default
state is listening, because state sync depends on overhearing the remote. An outgoing
command is queued, sent as a burst across the three channels, and the radio returns to
listening. Nothing else touches the PL1167.

**`Net`** — WiFi and OTA together, not separately: OTA can only start once WiFi is up, and
splitting them puts that ordering across a class boundary for no gain.

**`Control`** — the facade. Owns `Encoder`, `LightState` and a `PacketSink`. Exposes
intent: `turnOn()`, `setBrightness(0..100)`, `setHue()`, `setWhite()`. Feeds every packet,
sent or overheard, into `LightState::apply`.

**`HaMqtt`** — discovery payload, topic subscriptions, and translation between Home
Assistant vocabulary and `Control` calls.

## Two rules about dependencies

**Control does not know MQTT exists.** It speaks protocol units and intent. Home Assistant
vocabulary stops at `HaMqtt`. Without this line the facade becomes the place everything
lands and none of it is testable off-device.

**Control reaches the radio through a one-method interface**, not `RadioLink` directly:

```cpp
struct PacketSink {
  virtual void send(const uint8_t *packet) = 0;
};
```

`RadioLink` implements it on hardware; tests inject a fake and assert on the bytes. One
virtual call per command is free, and it makes the whole control layer natively testable.

## Home Assistant specifics

Use the **JSON schema** — one payload carrying state, brightness, colour mode and colour,
rather than a topic per property.

Declare **`brightness_scale: 100`** in the discovery config. Home Assistant then speaks our
0–100 units natively and a whole class of rounding bugs disappears. `supported_color_modes`
is `["rgb", "color_temp"]`; note that `onoff` and `brightness` may only appear alone.

Colour temperature is the one genuinely lossy conversion: Home Assistant uses mireds or
kelvin, the protocol uses an opaque 0–100. Map it, test the endpoints, and do not pretend
it is exact.

Availability is an MQTT last will, set at connect time. A crashed bridge is otherwise
indistinguishable from an idle one, which is the P6 requirement and nearly free here.

## Memory

The ESP8266 has roughly 48 KB of free heap once WiFi and OTA are up. Use fixed-size
JSON documents and avoid `String` in anything that runs per packet.

## Consequence for the diagnostic sketches

`src/tx_test.cpp` exists only because the firmware could not transmit. Once `Control` can,
it is dead and gets deleted. `radio_selftest` and `sniffer` stay: they answer questions the
firmware cannot, and both are still useful when the radio misbehaves.
