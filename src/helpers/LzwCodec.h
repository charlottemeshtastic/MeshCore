#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mesh {

/** Bounded message codec: fixed 9-bit MSB-first codes, 512 dictionary entries. */
class LzwCodec {
public:
  static constexpr size_t MAX_DECODED_SIZE = 184;
  static constexpr size_t MAX_ENCODED_SIZE = 210;

  enum Status : uint8_t { OK, INVALID_INPUT, OUTPUT_TOO_SMALL };
  struct Result {
    Status status;
    size_t length;
  };

  /** Encode one independent stream; buffers must not overlap. Partial output on failure is unusable. */
  static Result encode(const uint8_t* src, size_t length, uint8_t* dest, size_t capacity);

  /** Decode one stream with zero padding and no trailing bytes; output is capped at 184 bytes. */
  static Result decode(const uint8_t* src, size_t length, uint8_t* dest, size_t capacity);
};

}
