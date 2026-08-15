#pragma once

#include <stdint.h>
#include <stdio.h>

// "pool-lights-" + 6 hex digits + NUL.
#define NETID_LEN 19

// Stable name for OTA discovery and the DHCP reservation: the chip id is the only value
// that survives a reflash. Masked to 24 bits so a full word cannot overflow NETID_LEN.
inline void deviceName(uint32_t chipId, char *out, size_t len) {
  snprintf(out, len, "pool-lights-%06x", (unsigned)(chipId & 0xFFFFFF));
}
