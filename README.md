# pool-lights

An ESP8266 + NRF24L01+ bridge that puts a 2.4 GHz MiLight / MiBoxer RF light — in this
case a pool light with no smart interface of any kind — onto Home Assistant over MQTT.

The light is driven by a MiBoxer RF receiver paired to a handheld remote. It speaks
neither WiFi nor Zigbee nor infrared, so Home Assistant cannot see it. A small ESP8266
with a 2.4 GHz radio can: it listens to the existing remote, learns the pairing, and then
emulates it — leaving the physical remote working exactly as before.

## Status

Rung 7 of 7. The board joins WiFi, accepts OTA updates, sniffs a MiLight remote's identity
off the air, **drives the real light**, and tracks state in both directions — including
presses on the physical remote. It serves a mobile-first debug UI at
`http://pool-lights.local/`, and exposes itself to Home Assistant over MQTT discovery as a
light plus two buttons. What is left is the permanent install and monitoring. See
[`docs/roadmap.md`](docs/roadmap.md) for the ladder and [`docs/plan.md`](docs/plan.md) for
the phase plan with exit criteria.

## Hardware

| Part | Notes |
|---|---|
| Wemos D1 R1 (ESP8266) | PlatformIO board id `d1`. Any ESP8266 works; the pin map here is R1-specific |
| NRF24L01+ (PA+LNA variant) | Needs 3.3 V and bulk capacitance at the module — see `docs/hardware.md` |
| 7 dupont wires | CE, CSN, SCK, MOSI, MISO, VCC, GND. IRQ unused |
| 10–100 µF + 100 nF caps | Across the radio's VCC/GND. **Not optional** — without them this build received flawlessly and transmitted nothing |

## Quick start

```bash
# macOS, Apple Silicon: the xtensa compiler is x86_64-only
softwareupdate --install-rosetta --agree-to-license

brew install platformio
git clone git@github.com:lucaswall/pool-lights.git ~/Projects/pool-lights
cd ~/Projects/pool-lights

make ports               # expect /dev/cu.usbserial-* (VID:PID 1A86:7523)
make test                # desktop unit tests, no board needed
make run                 # build, flash, and print the boot banner
```

WiFi and OTA need `include/secrets.h` — copy `include/secrets.h.example` and fill it in.
Once a board is running this firmware it can be reflashed over the air:

```bash
make ota OTA_HOST=pool-lights.local
```

OTA only works on a board whose *running*
sketch handles OTA, so one bad flash puts you back on USB.

`make help` lists every target. A correct build prints
`pinmap : D2=16 D4=4 D8=0 D10=15 LED_BUILTIN=2`; different numbers mean the wrong board
variant was compiled.

## Layout

```
platformio.ini      two envs: d1 (hardware) and native (tests)
src/                firmware and the debug web UI; radio self-test and sniffer, one env each
lib/PL1167/         PL1167 transceiver on the NRF24
include/            header-only pure logic, unit tested
test/               desktop unit tests (pio test -e native)
tools/              serial capture, privacy scan
docs/               plan, roadmap, hardware, protocol notes, code standards
local/              gitignored: site-specific values, real IPs, sniffed device IDs
.claude/            Claude Code project settings
```

## Contributing

[`docs/code-standards.md`](docs/code-standards.md) is binding: minimal code, no dead
things, tests first for anything with a definable input and output, and never leave the
repository dirty. Run `make check` before publishing anything.

## A note on privacy

This repository is written to be publishable. Nothing in it identifies a particular home,
network or installation: no IPs, no credentials, no MAC addresses, and no RF pairing IDs —
those last ones are effectively the keys to someone's light. Anything site-specific lives
in the gitignored `local/` directory. See RULE 0 in `CLAUDE.md`.

## Credits and prior art

The eventual firmware is [`sidoh/esp8266_milight_hub`](https://github.com/sidoh/esp8266_milight_hub),
which does the real protocol work. This repository is the incremental path to running it
on this hardware, plus whatever local patches turn out to be necessary.

## Licence

MIT — see [`LICENSE`](LICENSE). Same as the upstream firmware this project exists to run.

Note for anyone redistributing a *built* binary: the RF24 library is GPL-2.0, so the
combined firmware image carries GPL-2.0 obligations even though this source is MIT.
Distributing the source alone does not.
