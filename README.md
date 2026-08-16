# pool-lights

An ESP8266 + NRF24L01+ bridge that puts a 2.4 GHz MiLight / MiBoxer RF light — in this
case a pool light with no smart interface of any kind — onto Home Assistant over MQTT.

The light is driven by a MiBoxer RF receiver paired to a handheld remote. It speaks
neither WiFi nor Zigbee nor infrared, so Home Assistant cannot see it. This bridge can:

```
FUT088-style remote ──2.4 GHz RF──┐
                                  ├──> MiBoxer receiver ──> the light
   this bridge ───────────────────┘
        │
        └── MQTT discovery ──> Home Assistant
```

It is a **second remote**, not a gateway in front of the light. It transmits the same
protocol with the same identity the physical remote uses, learned by listening to it, so
both keep working and neither is aware of the other. It also keeps listening, so pressing
the physical remote updates Home Assistant.

Everything except the radio protocol runs on a Wemos D1 R1: WiFi, OTA, a debug web UI at
`http://pool-lights.local/`, and the MQTT integration.

## What it exposes

One Home Assistant device, three entities:

- **Light** — on/off, brightness, RGB colour, colour temperature, five effects
- **Effect faster** / **Effect slower** — buttons, because those keys are momentary
  actions with no state

Plus a retained availability topic backed by an MQTT last will, so a crashed bridge shows
as unavailable rather than as a light that has quietly stopped responding.

### What it cannot know

The RF protocol is **one-way**. The receiver never reports anything — not its state, not
an acknowledgement, not its presence. Everything Home Assistant shows is inferred from
what the bridge sent plus what it overheard:

- A command lost to interference leaves Home Assistant optimistic and wrong until the next
  command. Retransmission helps; certainty is not available.
- If the light is switched at the wall, the bridge has no way to know.
- The only proof a command landed is a person looking at the light.

## Hardware

| Part | Notes |
|---|---|
| Wemos D1 R1 (ESP8266) | PlatformIO board id `d1`. Any ESP8266 works; the pin map here is R1-specific |
| NRF24L01+ (PA+LNA variant) | 3.3 V only. Wiring in [`docs/hardware.md`](docs/hardware.md) |
| 7 dupont wires | CE, CSN, SCK, MOSI, MISO, VCC, GND. IRQ unused |
| 10–100 µF + 100 nF caps | Across the radio's VCC/GND. **Not optional** — without them this build received flawlessly and transmitted nothing at all |

## Quick start

```bash
# macOS, Apple Silicon: the xtensa compiler is x86_64-only
softwareupdate --install-rosetta --agree-to-license
brew install platformio

cp include/secrets.h.example include/secrets.h   # then fill it in
make test          # desktop unit tests, no board needed
make run           # build, flash, print the boot banner
```

`secrets.h` carries the WiFi and MQTT credentials, the OTA password, and the light's RF
identity. Capture that identity with `make sniff` and press buttons on the physical
remote; it prints the device id and group.

Once a board is running this firmware, reflash it over the air with
`make ota OTA_HOST=pool-lights.local`. That only works while the *running* sketch handles
OTA, so one bad flash puts you back on USB.

`make help` lists every target.

## HTTP endpoints

Everything the board serves, on port 80.

| Endpoint | Purpose |
|---|---|
| `GET /` | The web UI: state, controls, and a live console panel |
| `GET /api/state` | State as JSON, plus IP, RSSI, uptime and heap. Polled twice a second by the page |
| `POST /api/set` | Commands. Every parameter is optional and applied if present: `on=1`, `off=1`, `white=1`, `hue=0..255`, `brightness=0..100`, `sat=0..100`, `effect=0..4` |
| `GET /status` | Snapshot: build stamp, reset reason, uptime, heap, WiFi, light state, and how full both rings are. Plain text, or JSON with `?json=1` |
| `GET /log` | The console ring as plain text, oldest first |
| `GET /errors` | Faults only, from a separate smaller ring |

`/api/state` carries a `version` counter that advances only when the light state really
changes, so a client can tell a change from a repeat without diffing.

```bash
curl http://pool-lights.local/status
curl "http://pool-lights.local/status?json=1" | jq .
curl -X POST "http://pool-lights.local/api/set?on=1&hue=85&brightness=60"
```

### The two rings

The console ring holds 80 lines, the fault ring 24, and faults are written to both. The
main log fills with routine traffic — a `health` line every five minutes, every command
sent and every remote press overheard — so a fault from hours ago would be long evicted by
the time anyone went looking for it.

Lines are stamped with uptime, and consecutive repeats collapse to `(xN)` so one retry loop
cannot evict everything else. The five-minute `health` line carries the fault count, so a
problem is visible from the main log without opening `/errors`. Free heap is reported whenever it reaches a new low: a
healthy board stays quiet, a leaking one shows a steady descent.

Both rings are RAM and start empty after a restart. `/status` exists for that reason — it
is computed on request rather than remembered, so the build stamp and reset reason cannot
scroll out of anything.

### If a name lookup seems to hang

Use the IP. The ESP8266 mDNS responder answers `A` queries but ignores `AAAA` entirely,
without even a negative reply, so a resolver asking for both waits out its full timeout —
about five seconds — on the IPv6 half before using the IPv4 answer it already had.

## Layout

```
include/        pure logic, header-only, unit tested — protocol codec, state, control
lib/PL1167/     PL1167 transceiver emulated on the NRF24
src/            peripherals and wiring: radio, WiFi/OTA, web UI, MQTT
                plus two standalone diagnostics with their own build envs
test/           desktop unit tests (make test)
tools/          bounded serial capture, privacy scan
docs/           hardware wiring, protocol notes, code standards
local/          gitignored: credentials, IPs, the sniffed device id
```

`pio test` does not build `src/`, which is why anything worth testing lives in a header
under `include/`. That split is the reason the protocol codec, the state machine and the
control layer are all verifiable without a radio attached.

Two diagnostics exist because the firmware cannot answer their questions: `make radio`
proves the radio is wired and genuine before any protocol code runs, and `make sniff` is
receive-only, so it can learn a remote's identity with no risk of transmitting a pairing
command.

## Contributing

[`docs/code-standards.md`](docs/code-standards.md) is binding: minimal code, no dead
things, tests first for anything with a definable input and output, and never leave the
repository dirty. Run `make check` before publishing anything.

## A note on privacy

This repository is written to be publishable. Nothing in it identifies a particular home,
network or installation: no IPs, no credentials, no MAC addresses, and no RF pairing IDs —
those last ones are effectively the keys to someone's light. Anything site-specific lives
in the gitignored `local/` directory. See RULE 0 in `CLAUDE.md`.

## Credits

The protocol constants — the V2 offset table, the xorKey derivation, the PL1167 register
sequences — are ported from [`sidoh/esp8266_milight_hub`](https://github.com/sidoh/esp8266_milight_hub)
(MIT), which took them in turn from [`henryk/openmili`](https://github.com/henryk/openmili).
Years of reverse engineering went into those and they cannot be re-derived. The code around
them is this project's own.

## Licence

MIT — see [`LICENSE`](LICENSE).

Note for anyone redistributing a *built* binary: the RF24 library is GPL-2.0, so the
combined firmware image carries GPL-2.0 obligations even though this source is MIT.
Distributing the source alone does not.
