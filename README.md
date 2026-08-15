# pool-lights

An ESP8266 + NRF24L01+ bridge that puts a 2.4 GHz MiLight / MiBoxer RF light — in this
case a pool light with no smart interface of any kind — onto Home Assistant over MQTT.

The light is driven by a MiBoxer RF receiver paired to a handheld remote. It speaks
neither WiFi nor Zigbee nor infrared, so Home Assistant cannot see it. A small ESP8266
with a 2.4 GHz radio can: it listens to the existing remote, learns the pairing, and then
emulates it — leaving the physical remote working exactly as before.

## Status

Rung 1 of 7. This repository currently contains a blink-and-serial sketch whose only job
is to prove the toolchain. Everything else is documented and not yet built. See
[`docs/roadmap.md`](docs/roadmap.md) for the ladder and [`docs/plan.md`](docs/plan.md) for
the phase plan with exit criteria.

## Hardware

| Part | Notes |
|---|---|
| Wemos D1 R1 (ESP8266) | PlatformIO board id `d1`. Any ESP8266 works; the pin map here is R1-specific |
| NRF24L01+ (PA+LNA variant) | Needs 3.3 V and bulk capacitance at the module — see `docs/hardware.md` |
| 7 dupont wires | CE, CSN, SCK, MOSI, MISO, VCC, GND. IRQ unused |
| 10–100 µF + 100 nF caps | Across the radio's VCC/GND. Not optional on PA+LNA |

## Quick start

```bash
# macOS, Apple Silicon: the xtensa compiler is x86_64-only
softwareupdate --install-rosetta --agree-to-license

brew install platformio
git clone git@github.com:lucaswall/pool-lights.git ~/Projects/pool-lights
cd ~/Projects/pool-lights

pio device list          # expect /dev/cu.usbserial-* (VID:PID 1A86:7523)
pio run                  # first build downloads the toolchain, ~400 MB, once
pio run -t upload
pio device monitor       # 115200; Ctrl-] to exit
```

A correct build prints `pinmap : D2=16 D4=4 D8=0 D10=15 LED_BUILTIN=2`. Different numbers
mean the wrong board variant was compiled.

## Layout

```
platformio.ini      one env: d1
src/main.cpp        current rung
docs/               plan, roadmap, hardware, protocol notes, HA integration
local/              gitignored: site-specific values, real IPs, sniffed device IDs
.claude/            Claude Code project settings
```

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

Not yet chosen — will be settled before the repository is made public.
