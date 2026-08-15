#pragma once

#include <stdint.h>
#include <stdio.h>

// "pool-lights-" + 6 hex digits + NUL.
#define NETID_LEN 19

// Stable per-board name. OTA discovery, the DHCP reservation and later the MQTT client id
// all need a name that survives a reflash, and the chip id is the only such value the
// board knows about itself. Masked to 24 bits so a full 32-bit word cannot overflow the
// buffer callers size with NETID_LEN.
inline void deviceName(uint32_t chipId, char *out, size_t len) {
  snprintf(out, len, "pool-lights-%06x", (unsigned)(chipId & 0xFFFFFF));
}
