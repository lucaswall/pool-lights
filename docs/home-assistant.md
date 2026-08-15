# Home Assistant integration

Written generically: it assumes a Home Assistant instance with an MQTT broker, and nothing
about a particular installation. Fill your own values into `local/site.md`.

## Shape of the integration

```
FUT088-style remote ──2.4 GHz RF──┐
                                  ├──> MiBoxer RF receiver ──> the light
ESP8266 + NRF24 (this project) ───┘
        │
        └── MQTT ──> broker ──> Home Assistant (MQTT discovery) ──> light entity
```

The bridge is a second remote, not a gateway in front of the light. It transmits on the
same protocol with the same pairing the physical remote uses, so both keep working and
neither is aware of the other.

## Why MQTT discovery rather than a REST integration

- Home Assistant's MQTT integration creates the entity automatically from a discovery
  payload; no YAML to maintain, no custom component to install, nothing to update when
  Home Assistant changes its config-flow rules.
- The bridge also *listens*. When someone presses the physical remote, the bridge overhears
  the packet and publishes the resulting state, so the entity does not drift out of sync.
  A polled REST integration cannot do this — there is nothing to poll.

## State, and its honest limits

The RF protocol is **one-way**. The receiver never reports anything: not its state, not an
acknowledgement, not even its presence. Everything Home Assistant shows is inferred from
what the bridge sent plus what it overheard. Consequences worth designing around:

- A command lost to interference leaves Home Assistant optimistic and wrong until the next
  command. Retransmission helps; certainty is not available.
- The only proof that a command landed is a human looking at the light. Any acceptance
  test for this project ends with someone at the poolside.
- If the light is switched at the wall, the bridge has no way to know.

## Entity expectations

A single light entity supporting on/off, brightness, colour and colour temperature,
depending on what the receiver implements. Name it and assign it an area in Home
Assistant; keep the real entity id out of this repository.

## Availability

An ESP that has crashed looks exactly like an ESP with nothing to say. Configure an MQTT
last-will on the bridge and have Home Assistant treat it as the availability topic, so a
dead bridge shows as unavailable rather than as a light that has quietly stopped
responding.

## Broker hygiene

The broker is usually shared with other integrations. Use a dedicated topic prefix, and do
not subscribe to unrelated trees.
