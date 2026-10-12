#include "Packet.h"
#include <string.h>
#include <SHA256.h>

namespace mesh {

Packet::Packet() {
  header = 0;
  path_len = 0;
  payload_len = 0;
  transport_codes[0] = transport_codes[1] = 0;
}

bool Packet::isValidPathLen(uint8_t path_len) {
  uint8_t hash_count = path_len & 63;
  uint8_t hash_size = (path_len >> 6) + 1;
  if (hash_size == 4) return false;  // Reserved for future
  return hash_count*hash_size <= MAX_PATH_SIZE;
}

size_t Packet::writePath(uint8_t* dest, const uint8_t* src, uint8_t path_len) {
  uint8_t hash_count = path_len & 63;
  uint8_t hash_size = (path_len >> 6) + 1;
  size_t len = hash_count*hash_size;
  if (!isValidPathLen(path_len)) {
    MESH_DEBUG_PRINTLN("Packet::copyPath, invalid path_len=%d", (uint32_t)path_len);
    return 0;   // Error
  }
  memcpy(dest, src, len);
  return len;
}

uint8_t Packet::copyPath(uint8_t* dest, const uint8_t* src, uint8_t path_len) {
  writePath(dest, src, path_len);
  return path_len;
}

bool Packet::readPath(uint8_t dest[], const uint8_t src[], size_t available, uint8_t& path_len) {
  if (dest == NULL || src == NULL || available == 0 || !isValidPathLen(src[0])) return false;
  size_t bytes = (src[0] & 63) * ((src[0] >> 6) + 1);
  if (available < 1 + bytes) return false;
  memcpy(dest, src + 1, bytes);
  path_len = src[0];
  return true;
}

int Packet::getRawLength() const {
  return 2 + getPathByteLen() + payload_len + (hasTransportCodes() ? 4 : 0);
}

void Packet::calculatePacketHash(uint8_t* hash) const {
  SHA256 sha;
  uint8_t t = getPayloadType();
  sha.update(&t, 1);
  if (t == PAYLOAD_TYPE_TRACE) {
    sha.update(&path_len, sizeof(path_len));   // CAVEAT: TRACE packets can revisit same node on return path
  }
  sha.update(payload, payload_len);
  sha.finalize(hash, MAX_HASH_SIZE);
}

uint8_t Packet::writeTo(uint8_t dest[], size_t capacity) const {
  if (dest == NULL || path_len > 255 || !isValidPathLen(path_len) ||
      payload_len > sizeof(payload) || getRawLength() > MAX_TRANS_UNIT ||
      static_cast<size_t>(getRawLength()) > capacity) return 0;
  uint8_t i = 0;
  dest[i++] = header;
  if (hasTransportCodes()) {
    for (int c = 0; c < 2; c++) {
      dest[i++] = transport_codes[c] & 0xFF;
      dest[i++] = transport_codes[c] >> 8;
    }
  }
  dest[i++] = path_len;
  i += writePath(&dest[i], path, path_len);
  memcpy(&dest[i], payload, payload_len); i += payload_len;
  return i;
}

bool Packet::readFrom(const uint8_t src[], size_t len) {
  if (src == NULL || len < 2 || len > MAX_TRANS_UNIT) return false;
  size_t i = 0;
  header = src[i++];
  if (getPayloadVer() != PAYLOAD_VER_1) return false;
  if (hasTransportCodes()) {
    if (len < 6) return false;
    for (int c = 0; c < 2; c++) {
      transport_codes[c] = src[i] | (uint16_t(src[i + 1]) << 8);
      i += 2;
    }
  } else {
    transport_codes[0] = transport_codes[1] = 0;
  }
  path_len = src[i++];
  if (!isValidPathLen(path_len)) return false;   // bad encoding

  uint8_t bl = getPathByteLen();
  if (i + bl > len) return false;
  memcpy(path, &src[i], bl); i += bl;

  payload_len = len - i;
  if (payload_len > sizeof(payload)) return false;  // bad encoding
  memcpy(payload, &src[i], payload_len); //i += payload_len;
  return true;   // success
}

}
