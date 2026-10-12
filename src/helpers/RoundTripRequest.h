#pragma once

#include <Packet.h>
#include <string.h>

namespace mesh {

/** Fork-specific authenticated request envelope with a transaction-scoped reply path.
 * Callers must establish recipient support before sending. This is not a V2 header.
 */
class RoundTripRequest {
public:
  static constexpr uint8_t TYPE = 0xF0;
  static constexpr uint8_t REVISION = 1;
  static constexpr uint8_t CAPABILITY_TYPE = 0xF1;
  static constexpr size_t FEATURE_RECORD_SIZE = 9;

  struct Decoded {
    uint8_t path_len;
    uint8_t path[MAX_PATH_SIZE];
    const uint8_t* body;
    size_t body_len;
  };

  struct Command {
    uint8_t recipient[PUB_KEY_SIZE];
    uint8_t out_path_len;
    uint8_t out_path[MAX_PATH_SIZE];
    Decoded request;
  };

  /** Write CMF, schema 1, then revisions for manual requests, profiles, LZW messages,
   * virtual nodes and adaptive CR. Zero means that feature is not supported.
   */
  static size_t writeFeatureRecord(uint8_t* dest, size_t capacity) {
    if (dest == nullptr || capacity < FEATURE_RECORD_SIZE) return 0;
    const uint8_t record[] = {'C', 'M', 'F', 1, REVISION, 0, 0, 0, 0};
    memcpy(dest, record, sizeof(record));
    return sizeof(record);
  }

  /** Peer response includes the request's little-endian timestamp/tag. */
  static size_t writeCapabilities(uint8_t* dest, size_t capacity, uint32_t tag) {
    if (dest == nullptr || capacity < 4 + FEATURE_RECORD_SIZE) return 0;
    for (int i = 0; i < 4; i++) dest[i] = tag >> (8 * i);
    return 4 + writeFeatureRecord(dest + 4, capacity - 4);
  }

  /** Encode [type, revision, path metadata, path bytes, body length, body].
   * Buffers must not overlap. Zero indicates invalid input or insufficient capacity.
   */
  static size_t encode(uint8_t* dest, size_t capacity, const uint8_t* path,
                       uint8_t path_len, const uint8_t* body, size_t body_len) {
    if (!Packet::isValidPathLen(path_len) || body == nullptr || body_len == 0 ||
        body_len > MAX_PACKET_PAYLOAD || body[0] == TYPE || dest == nullptr) return 0;
    size_t bytes = (path_len & 63) * ((path_len >> 6) + 1);
    size_t total = 4 + bytes + body_len;
    if (total > capacity || total > MAX_PACKET_PAYLOAD || (bytes && path == nullptr)) return 0;
    dest[0] = TYPE;
    dest[1] = REVISION;
    dest[2] = path_len;
    if (bytes) memcpy(dest + 3, path, bytes);
    dest[3 + bytes] = static_cast<uint8_t>(body_len);
    memcpy(dest + 4 + bytes, body, body_len);
    return total;
  }

  /** Decode a complete envelope; trailing cipher padding must be zero.
   * Outputs remain unchanged on failure. body borrows src until its handler returns.
   */
  static bool decode(const uint8_t* src, size_t available, Decoded& dest) {
    if (src == nullptr || available < 4 || available > MAX_PACKET_PAYLOAD ||
        src[0] != TYPE || src[1] != REVISION) return false;
    return decodeContent(src + 2, available - 2, dest, true);
  }

  /** Parse companion command F0, revision, confirmed-support flag (1), full recipient
   * key, outbound path, return path, body length and body. Frames have no padding.
   */
  static bool decodeCommand(const uint8_t* src, size_t available, Command& dest) {
    constexpr size_t prefix = 3 + PUB_KEY_SIZE;
    if (src == nullptr || available < prefix + 4 || available > MAX_TRANS_UNIT || src[0] != TYPE ||
        src[1] != REVISION || src[2] != 1) return false;
    Command candidate{};
    memcpy(candidate.recipient, src + 3, PUB_KEY_SIZE);
    if (!Packet::readPath(candidate.out_path, src + prefix, available - prefix, candidate.out_path_len)) return false;
    size_t offset = prefix + 1 + (candidate.out_path_len & 63) * ((candidate.out_path_len >> 6) + 1);
    if (!decodeContent(src + offset, available - offset, candidate.request, false)) return false;
    dest = candidate;
    return true;
  }

private:
  static bool decodeContent(const uint8_t* src, size_t available, Decoded& dest, bool allow_padding) {
    if (available < 2) return false;
    Decoded candidate{};
    if (!Packet::readPath(candidate.path, src, available, candidate.path_len)) return false;
    size_t offset = 1 + (candidate.path_len & 63) * ((candidate.path_len >> 6) + 1);
    if (offset >= available) return false;
    candidate.body_len = src[offset++];
    if (candidate.body_len == 0 || candidate.body_len > MAX_PACKET_PAYLOAD ||
        candidate.body_len > available - offset || src[offset] == TYPE) return false;
    candidate.body = src + offset;
    if (!allow_padding && offset + candidate.body_len != available) return false;
    for (size_t i = offset + candidate.body_len; i < available; i++) {
      if (src[i] != 0) return false;
    }
    dest = candidate;
    return true;
  }
};

}
