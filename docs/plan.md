# Plan

Public-safe mirror of the working plan. Phases, exit criteria and verification only —
every site-specific value (IPs, MACs, credentials, sniffed RF identifiers) lives in
`local/site.md` and never here. See RULE 0 in `CLAUDE.md`.

## Goal

Put a 2.4 GHz MiLight/MiBoxer pool light on Home Assistant as a first-class light entity,
without touching anything at the pool: no re-pairing, no rewiring, no replacement of the
RF receiver, and with the physical remote still working afterwards.

## Success criteria

- On/off, brightness and colour all work from Home Assistant
- Presses on the physical remote are reflected in Home Assistant state
- ≥95% of 20 consecutive commands succeed from the bridge's permanent location
- The bridge survives a power cut and a Home Assistant restart with no manual steps
- The bridge going offline is detected by monitoring, not by someone noticing the light
  stopped responding

## The human parts

This plan cannot be executed end to end by an agent. Three things are physical:
assembling the board and radio, siting the bridge within RF range, and looking at the
light during verification. The last one is not a gap in the tooling — the protocol is
one-way, the receiver reports nothing, and no instrumentation on the transmit side can
prove a command landed.

## Phases

| Phase | Title | Exit criteria |
|---|---|---|
| P0 | Hardware | Board enumerates over USB; radio wired by GPIO number with decoupling caps and antenna fitted; a candidate mounting spot named |
| P1 | Dev environment + hello world | `pio run -t upload` works; serial banner prints the `pinmap` line proving the right board variant; repo tagged `step-01-blink` |
| P2 | Build and flash the bridge | Radio self-test passes; firmware built from source (1.13.1-beta2, with a `d1` env added); web UI reachable; radio pins set; three clean power cycles |
| P3 | Sniff and identify | Device ID / type / group captured from the physical remote; a saved alias reproduces on/off and a colour change on the real light |
| P4 | MQTT + Home Assistant | Light entity exists, controls the light, and updates when the physical remote is used; survives an HA restart |
| P5 | Permanent install | 20 consecutive commands succeed from the final location; unattended recovery from a power cut; DHCP reservation in place |
| P6 | Monitoring + documentation | An induced outage raises the alert path and clears on recovery; findings written up |

## Phase notes

### P0 — Hardware

Wire by GPIO number, never by silkscreen label (`docs/hardware.md`). Decoupling caps at
the radio module are mandatory on the PA+LNA variant. Antenna on before power.

### P1 — Dev environment + hello world

Exists so that the first time something goes wrong, it is a five-line sketch failing and
not a twenty-thousand-line firmware. Board id `d1`; the `pinmap` banner line is the
verification that matters.

### P2 — Build and flash the bridge

Two steps in order. First a **standalone radio self-test** — the upstream firmware never
verifies the radio answered, so an unwired module presents as a healthy device on WiFi and
MQTT. Then the firmware itself, built from source as a sibling clone (never a submodule),
pinned to 1.13.1-beta2 for the FUT088 colour-temperature and saturation commands.

### P3 — Sniff and identify

Read-only against the receiver: listen and replay, never pair. If nothing is captured, the
cause is almost always power or listen-channel configuration — check those before
suspecting the protocol. The captured identifiers are RF credentials for a real light:
`local/site.md`, never the repo.

Verification requires someone watching the light.

### P4 — MQTT + Home Assistant

Discovery only, no hand-written YAML entity. Verify both directions and count the misses
over 20 commands — the number is the baseline P5 has to match from the final location.

### P5 — Permanent install

Move, power, enclose, then re-run the count *there*. If the spot fails, try another spot
before reaching for an RF repeater. Do not lower the bar to fit the location.

### P6 — Monitoring + documentation

A crashed bridge is indistinguishable from an idle one, so availability needs an explicit
signal (MQTT last-will) rather than an absence of complaints. Induce an outage to prove
the alert fires.

## Decisions

| # | Decision | Rationale |
|---|---|---|
| D1 | Self-hosted ESP bridge over the vendor's WiFi gateway | The vendor gateway routes through a cloud service; the bridge is local, uses parts on hand, and additionally gives state sync by overhearing the physical remote |
| D2 | Do not replace the RF receiver with a Zigbee/WiFi controller | Wiring work at the pool for no functional gain |
| D3 | CSN on a non-strap GPIO rather than the firmware default | GPIO15 must be low at boot; a CSN line idling high there can prevent booting on boards without a pulldown |
| D4 | Clone the remote's identity by sniffing rather than pairing as a new remote | Nothing at the receiver is touched, the physical remote keeps working, and no existing pairing can be displaced |
| D5 | Human visual confirmation is an accepted verification step | The protocol is one-way. This is a property of the system, not a defect in the plan |
| D6 | Build from source rather than flashing a release binary | The needed decoding exists only in a pre-release, upstream is dormant, and any remaining gap becomes a local patch |
| D7 | A standalone radio self-test precedes the bridge firmware | The firmware discards its radio's init result, so a wiring fault would otherwise be diagnosed as a protocol problem |
| D8 | Upstream firmware is a sibling clone, never a submodule | Keeps our incremental sketches and a large upstream tree independently versioned, and keeps upstream rebases out of this history |
