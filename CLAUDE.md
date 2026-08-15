# pool-lights

ESP8266 + NRF24L01+ bridge that puts a 2.4 GHz MiLight/MiBoxer RF light on Home Assistant
over MQTT. Built incrementally: it starts as a blink-and-serial sketch and grows one
verified rung at a time. See `docs/plan.md` for the phases and `docs/roadmap.md` for the
ladder.

---

## RULE 0 — THIS REPOSITORY WILL BE PUBLIC

It is private today and will be published later. Write every file as if it were already
public. This rule outranks convenience, and it applies to code, comments, commit messages,
docs, and issue text.

**Never commit, in any file or commit message:**

- WiFi SSIDs or passwords, MQTT usernames/passwords, API tokens, OTA passwords
- IP addresses of real hosts — LAN, VPN/Tailscale, or public. Use `<HUB_IP>` or RFC 5737
  documentation addresses (`192.0.2.x`) in examples
- Hostnames of real machines, and the owner's home network topology
- MAC addresses, and the ESP's chip ID
- **The sniffed MiLight device ID / group / remote pairing values.** These are the RF
  credentials of a real light: anyone within radio range who knows them can drive it
- Home Assistant instance URLs, long-lived tokens, or entity registries dumped verbatim
- Photos of the house, floor plans, geolocation, anything identifying the address
- Personal names, email addresses, phone numbers, purchase/invoice references

**Where site-specific values go instead:** `local/` — the whole directory is gitignored
except its README. Put real IPs, credentials, sniffed device IDs and personal notes in
`local/site.md`. Never `git add -f` anything under `local/`. Secrets that must reach the
firmware go in `include/secrets.h` (gitignored) from the WiFi rung onward, with
`include/secrets.h.example` committed carrying placeholders only.

**Before every commit:** if a value came from the real installation rather than from a
datasheet, it belongs in `local/`, not in the tree. When in doubt, leave it out and put a
placeholder.

**Keep the docs generic.** This targets Home Assistant, not one particular Home Assistant.
Write "your broker", "your HA instance", "the hub's IP" — never the real ones. A reader
with the same hardware should be able to follow the docs without knowing anything about
the author's house.

---

## Hardware

Wemos **D1 R1** (ESP8266, CH340G USB-serial, micro-USB, Uno form factor) + NRF24L01+PA+LNA.

PlatformIO board id is `d1`. **NOT `d1_mini`** — that is the R2/mini with a different pin
map; it compiles and uploads cleanly while every pin number is silently wrong.

### Pin map — the D1 R1 is NOT a D1 mini

```
D0=GPIO3   D1=GPIO1   D2=GPIO16  D3=GPIO5   D4=GPIO4   D5=GPIO14
D6=GPIO12  D7=GPIO13  D8=GPIO0   D9=GPIO2   D10=GPIO15
D11=GPIO13 D12=GPIO12 D13=GPIO14
```

D7 and D11 are the same pin (GPIO13). D5 and D13 are both GPIO14. D6 and D12 are both GPIO12.

- `LED_BUILTIN` = GPIO2 (pad D9), **active low**. A second board LED on GPIO14 is HSPI SCK —
  never build on it.
- Hardware SPI: SCK=GPIO14, MISO=GPIO12, MOSI=GPIO13.
- GPIO0 (pad D8) and GPIO15 (pad D10) are boot-strap pins. GPIO15 held high blocks boot.

**Always reason in GPIO numbers.** Every D1 mini and NodeMCU tutorial gives the wrong
D-numbers for this board.

## Commands

| Task | Command |
|---|---|
| Find the port | `pio device list` (expect `/dev/cu.usbserial-*`; never `/dev/tty.*`) |
| Build | `pio run` |
| Flash | `pio run -t upload` |
| Clean | `pio run -t clean` |
| Erase flash | `pio run -t erase` |
| Symbol index for clangd | `pio run -t compiledb` |
| Serial monitor (human, own terminal) | `pio device monitor` |

## The serial port is a single-holder resource

One process owns `/dev/cu.*` at a time. A live monitor makes upload fail with
`Could not exclusively lock port ... [Errno 35]`. PlatformIO will not stop it for you
(platformio-core#384, wontfix). Stop any reader before flashing, and never end a session
with one running. `lsof /dev/cu.usbserial-*` finds the holder.

## Never run `pio device monitor` from a tool call

It dies with `termios.error: (25, 'Inappropriate ioctl for device')` whenever stdin is not
a TTY, which is always true for an agent Bash call (platformio-core#5113, open). A
foreground call that times out gets backgrounded rather than killed, leaving an orphan
holding the port. `.claude/settings.json` denies it. Do not pass `-t monitor` to `pio run`
either. From the SPI rung onward, use `tools/serial_log.py`.

## Three different baud rates

- **115200** — our sketches (`Serial.begin(115200)`, `monitor_speed = 115200`)
- **74880** — the ESP8266 boot ROM banner. Garbage at 115200 during the first second after
  reset is expected, not a fault. Read it with `pio device monitor -b 74880`
- **9600** — `pio device monitor`'s own default when no speed is given, and also the
  milight-hub firmware's console. Cause of most "garbage on the monitor" reports

## No JTAG

The ESP8266 has no on-chip debug and `d1.json` declares no debug block; `pio debug` does
not work and no probe helps. Debugging is serial prints, the `esp8266_exception_decoder`
monitor filter, and offline `addr2line`. Do not set `build_type = debug` — upstream
milight-hub documents it causing stack smashing and watchdog resets.

## macOS notes

- **Apple Silicon needs Rosetta 2.** The xtensa compiler is x86_64-only and no arm64 build
  exists. `Bad CPU type in executable` / scons `Error 126` means it is missing:
  `softwareupdate --install-rosetta --agree-to-license`
- **Do not install a CH340 driver.** macOS ships Apple's own `AppleUSBCHCOM`, which matches
  this chip. WCH's driver adds a dead duplicate port and, on Apple Silicon, breaks the
  DTR/RTS auto-reset that flashing depends on. A `/dev/cu.wchusbserial*` node means a
  third-party driver is installed and should be removed

## Conventions

- Comments explain **why**, not what. Pin choices and timing constants always get a reason.
- One rung at a time: get it verified and committed before starting the next.
- Tag each completed rung (`step-01-blink`, `step-02-serial`, …) so a regression can be
  bisected against working hardware.
