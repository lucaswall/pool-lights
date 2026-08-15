# local/ — never committed

Everything in this directory except this README is gitignored. **This repository is
destined to be public** (see RULE 0 in `CLAUDE.md`), so anything that came from the real
installation lives here and nowhere else.

Create `local/site.md` on your machine with whatever the work needs:

```markdown
# site notes (LOCAL ONLY)

## Network
hub IP / hostname     : ...
DHCP reservation MAC  : ...
WiFi SSID             : ...            # password goes in include/secrets.h, not here

## MQTT
broker host / port    : ...
username              : ...            # password in include/secrets.h
topic prefix          : ...

## Home Assistant
base URL              : ...
entity id in use      : ...
area                  : ...

## RF (fill in after the sniff rung)
device id             : 0x....         # RF credentials of a real light — never commit
device type           : ...
group id              : ...
listen channel        : ...
measured range / RSSI at the mounting spot: ...
```

Rules of thumb:

- If a value came from a datasheet or an upstream project, it can be committed.
- If it came from *your* house, it belongs here.
- Never `git add -f` anything under `local/`.
