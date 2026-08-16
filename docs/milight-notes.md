# MiLight / MiBoxer protocol notes

What is known about the protocol, so the same research is not repeated. No values from any
real installation appear here — see RULE 0 in `CLAUDE.md`.

## The remote determines everything

MiBoxer sells many remotes and they are not interchangeable protocol-wise. The one this
project targets is a **FUT088**: single-zone RGB+CCT, full touch, 2.4 GHz RF, ~30 m range.
Layout: hue ring, saturation bar, brightness bar, then ON / W / OFF, R / G / B, and
S−/60s, M, S+/10min.

Identifying the remote identifies the receiver's protocol family, which is the only way in
— the receiver itself is unmarked and usually inaccessible.

## Where the constants came from

The V2 offset table, the xorKey derivation and the PL1167 register sequences are ported
from [`sidoh/esp8266_milight_hub`](https://github.com/sidoh/esp8266_milight_hub) (MIT),
which took them from [`henryk/openmili`](https://github.com/henryk/openmili). They encode
reverse-engineered silicon behaviour and cannot be derived from first principles, which is
why they are ported rather than rewritten. Everything around them here is this project's
own, and is tested — upstream has no unit tests at all.

Upstream is dormant: last master commit 2025-02-11, ~179 open issues. It is worth reading
as a reference and worth compiling as a control when something is inexplicable, but it is
not a dependency.

## FUT088 command map

Measured from 3322 captured packets (205 distinct) with `make sniff`, cross-checked
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
This remote uses the combined `0x07` instead.

## Sniffing, and its caveats

`make sniff` listens and reports packets from other remotes. That is how the identity is
learned: press buttons on the physical remote and read the device id, protocol and group
off the captured packets, then transmit as that identity. The receiver is never touched,
never re-paired, and the physical remote keeps working.

- The sniffer is **receive-only by construction** — no transmit path exists in that
  binary. That matters because **a mis-sent pairing command can displace the remote the
  receiver is bound to**, and pairing is repeated ON to group 0.
- It rotates all three channels rather than picking one. Upstream has reports of capturing
  nothing on a fixed channel, and rotating removes the guess.
- Presses are transmitted dozens of times each, so one press yields one deduplicated line
  and capture is not the bottleneck it is sometimes described as.

## Check the band before anything else

MiBoxer's underwater/pool product line in the FUT086 family is **433 MHz LoRa**, which an
NRF24 cannot touch at all. A 2.4 GHz remote is strong evidence the receiver is 2.4 GHz
too, but the sniff is what settles it: if pressing the remote produces no packets on a
radio that passed `make radio`, suspect the band before suspecting the wiring a second
time.
