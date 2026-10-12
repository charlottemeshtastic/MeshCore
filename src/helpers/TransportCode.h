#pragma once

#include <Packet.h>
#include <SHA256.h>

namespace mesh {

/** V1 scope token: HMAC-SHA256(type || encrypted payload), truncated little-endian. */
inline uint16_t calculateTransportCode(const uint8_t key[16], const Packet* packet) {
  uint8_t digest[2];
  SHA256 sha;
  sha.resetHMAC(key, 16);
  uint8_t type = packet->getPayloadType();
  sha.update(&type, 1);
  sha.update(packet->payload, packet->payload_len);
  sha.finalizeHMAC(key, 16, digest, sizeof(digest));
  uint16_t code = digest[0] | (uint16_t(digest[1]) << 8);
  if (code == 0) return 1;
  if (code == 0xFFFF) return 0xFFFE;
  return code;
}

}
