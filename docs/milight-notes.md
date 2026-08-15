# MiLight / MiBoxer protocol notes

Findings gathered 2026-08-15 from upstream source, issues and releases. They are recorded
here so the same research is not repeated. No values from any real installation appear in
this file — see RULE 0 in `CLAUDE.md`.

## The remote determines everything

MiBoxer sells many remotes and they are not interchangeable protocol-wise. The one this
project targets is a **FUT088**: single-zone RGB+CCT, full touch, 2.4 GHz RF, ~30 m range.
Layout: hue ring, saturation bar, brightness bar, then ON / W / OFF, R / G / B, and
S−/60s, M, S+/10min.

Identifying the remote identifies the receiver's protocol family, which is the only way in
— the receiver itself is unmarked and usually inaccessible.

## Firmware: pin 1.13.1-beta2, not 1.13.0

Upstream is [`sidoh/esp8266_milight_hub`](https://github.com/sidoh/esp8266_milight_hub).

- The FUT088 is a V2 protocol-ID `0x25` remote in the FUT088/089/092 family, handled as
  remote type **`fut089`**. No new packet formatter is needed.
- **But** its dedicated colour-temperature and saturation controls emit commands `0x03` and
  `0x04`, which release **1.13.0 and current master do not decode**. That decoding exists
  only on branch `1.13.1` / pre-release **1.13.1-beta2** (2024-12-11).
- Upstream is dormant: last master commit 2025-02-11, ~179 open issues, maintainer citing
  very limited time. Anything still missing becomes a local patch.

## Building it on a Wemos D1 R1

The upstream `platformio.ini` has **no `d1` environment** — it ships nodemcuv2, d1_mini,
esp12, esp07, huzzah, d1_mini_pro and esp32. Add four lines:

```ini
[env:d1]
extends = esp8266
board = d1
```

Also, when building it:

- Delete `extra_scripts = pre:.build_web.py`. The React web UI is already committed as
  gzipped C headers; leaving the hook in means every build silently runs `npm install` on
  ~60 packages, regenerates the headers and dirties the tree. Keep `!python3 .get_version.py`,
  which only needs git.
- Its console is **9600 baud**, not the 115200 this project's own sketches use.
- Its `[env:ota]` calls `curl.exe` and does not work outside Windows. There is no
  ArduinoOTA/espota listener at all: OTA is a multipart POST to `/firmware`, so
  `curl -F "image=@firmware.bin" http://<HUB_IP>/firmware`.
- Do not set `build_type = debug`; upstream documents it causing stack smashing and
  watchdog resets.
- Default radio pins are CE=GPIO4, CSN=GPIO15. Both are runtime settings, changeable via
  the settings API without reflashing — useful, because GPIO15 is a boot-strap pin and a
  poor choice for CSN on this board.

## FUT088 command map

Measured from 3322 captured packets (205 distinct) with our own sniffer, cross-checked
against upstream's `FUT089PacketFormatter`. Decoded V2 packet layout is:

```
[0] key  [1] protocol 0x25  [2..3] device id  [4] command  [5] argument
[6] sequence  [7] group  [8] checksum
```

| Control | Cmd | Argument | Notes |
|---|---|---|---|
| ON | `0x01` | `groupId` | group 1 → `0x01` |
| OFF | `0x01` | `groupId + 9` | group 1 → `0x0A` |
| W (white mode) | `0x01` | `0x14` | |
| S+ (mode speed up) | `0x01` | `0x12` | |
| S− (mode speed down) | `0x01` | `0x13` | |
| Hue ring | `0x02` | `0x00`–`0xFF` | raw = hue° × 255/360 |
| R / G / B buttons | `0x02` | `0x00` / `0x55` / `0xAB` | 0°, 120°, 240° on the circle |
| Brightness bar | `0x05` | `0`–`100` decimal | **percentage, not 0–255** |
| M (mode) | `0x06` | `0x00`–`0x04` | five modes |
| Saturation / kelvin bar | `0x07` | `0`–`100` decimal | saturation in colour mode, kelvin in white mode |

### Held buttons set bit 7 of the command

A long press does not use a separate command. It re-sends the same command with the top
bit set, so `0x01` becomes `0x81` and the argument still says which key was held. All four
confirmed by capture:

| Held key | Cmd | Arg | Effect |
|---|---|---|---|
| OFF | `0x81` | `0x0A` | night mode — dim nightlight glow, below the normal minimum |
| S− | `0x81` | `0x13` | 60 s sleep timer |
| S+ | `0x81` | `0x12` | 10 min sleep timer |
| ON | `0x81` | `0x01` | encoding confirmed, effect not identified |

**Upstream mis-decodes three of these.** `FUT089PacketFormatter::parsePacket` tests the
high bit before the argument, so *every* `0x81` packet is reported as night mode:

```cpp
if (command == FUT089_ON) {
  if ((packet[V2_COMMAND_INDEX] & 0x80) == 0x80) {
    result[COMMAND] = NIGHT_MODE;      // both timers and held-ON land here too
```

A decoder that wants the timers has to check the argument first.

Three things that will bite an implementer:

- **Brightness and saturation are 0–100, not 0–255.** Only hue uses the full byte.
- **The group byte at [7] is not reliable.** Upstream extracts the group from the
  *argument* of an ON/OFF command instead, and only trusts `[7]` otherwise.
- **Bit 7 of the command is the held flag**, so mask with `0x7F` before comparing.

Still unobserved: commands `0x03`/`0x04`, the FUT092-style separate kelvin and saturation.
This remote uses the combined `0x07` instead, which suggests the 1.13.1-beta2 pin above
may not have been necessary after all.

## Sniffing, and its caveats

The hub can listen passively and report packets from other remotes. That is how the
pairing is learned: press buttons on the physical remote, read the device ID, device type
and group off the captured packets, then emulate that identity. The receiver is never
touched, never re-paired, and the physical remote keeps working.

Known rough edges:

- Nothing is captured at the default listen channel setting in some setups; issue #847
  needed it raised.
- Capture reliability is roughly 50% on released firmware — press a button several times.
- Group 0 can be sniffed but not configured (#885, open).
- **Never send a pairing or unpairing command while experimenting.** A mis-sent pair can
  displace the remote the receiver is bound to.

## Check the band before anything else

MiBoxer's underwater/pool product line in the FUT086 family is **433 MHz LoRa**, which this
firmware and an NRF24 cannot touch at all (#828, declined). A 2.4 GHz remote is strong
evidence the receiver is 2.4 GHz too, but the sniff is what settles it: if pressing the
remote produces no packets on a radio proven good by self-test, suspect the band before
suspecting the wiring a second time.
