#include "LzwCodec.h"

namespace mesh {
namespace {
constexpr uint16_t CLEAR = 256;
constexpr uint16_t END = 257;
constexpr uint16_t FIRST = 258;
constexpr uint16_t LIMIT = 512;

class BitWriter {
  uint8_t* dest;
  size_t capacity, bits;
public:
  BitWriter(uint8_t* dest, size_t capacity) : dest(dest), capacity(capacity), bits(0) {}
  bool put(uint16_t code) {
    if ((bits + 9 + 7) / 8 > capacity) return false;
    for (int bit = 8; bit >= 0; bit--) {
      if (bits % 8 == 0) dest[bits / 8] = 0;
      dest[bits / 8] |= ((code >> bit) & 1) << (7 - bits % 8);
      bits++;
    }
    return true;
  }
  size_t size() const { return (bits + 7) / 8; }
};

class BitReader {
  const uint8_t* src;
  size_t length, bits;
public:
  BitReader(const uint8_t* src, size_t length) : src(src), length(length), bits(0) {}
  bool get(uint16_t& code) {
    if (bits + 9 > length * 8) return false;
    code = 0;
    for (int i = 0; i < 9; i++, bits++) {
      code = (code << 1) | ((src[bits / 8] >> (7 - bits % 8)) & 1);
    }
    return true;
  }
  bool validPadding() const {
    if ((bits + 7) / 8 != length) return false;
    for (size_t i = bits; i < length * 8; i++) {
      if ((src[i / 8] >> (7 - i % 8)) & 1) return false;
    }
    return true;
  }
};
}

LzwCodec::Result LzwCodec::encode(const uint8_t* src, size_t length, uint8_t* dest, size_t capacity) {
  if ((length && src == nullptr) || dest == nullptr || length > MAX_DECODED_SIZE) {
    return {INVALID_INPUT, 0};
  }
  uint16_t prefix[LIMIT - FIRST];
  uint8_t suffix[LIMIT - FIRST];
  uint16_t next = FIRST;
  BitWriter writer(dest, capacity);
  if (!writer.put(CLEAR)) return {OUTPUT_TOO_SMALL, 0};
  if (length) {
    uint16_t word = src[0];
    for (size_t i = 1; i < length; i++) {
      uint16_t found = LIMIT;
      for (uint16_t c = FIRST; c < next; c++) {
        if (prefix[c - FIRST] == word && suffix[c - FIRST] == src[i]) {
          found = c;
          break;
        }
      }
      if (found != LIMIT) {
        word = found;
      } else {
        if (!writer.put(word)) return {OUTPUT_TOO_SMALL, 0};
        if (next < LIMIT) {
          prefix[next - FIRST] = word;
          suffix[next - FIRST] = src[i];
          next++;
        }
        word = src[i];
      }
    }
    if (!writer.put(word)) return {OUTPUT_TOO_SMALL, 0};
  }
  if (!writer.put(END)) return {OUTPUT_TOO_SMALL, 0};
  return {OK, writer.size()};
}

LzwCodec::Result LzwCodec::decode(const uint8_t* src, size_t length, uint8_t* dest, size_t capacity) {
  if (src == nullptr || dest == nullptr || length > MAX_ENCODED_SIZE) return {INVALID_INPUT, 0};
  uint16_t prefix[LIMIT - FIRST];
  uint8_t suffix[LIMIT - FIRST], stack[MAX_DECODED_SIZE];
  uint16_t next = FIRST, code, previous = LIMIT;
  uint8_t first = 0;
  size_t output = 0;
  BitReader reader(src, length);
  if (!reader.get(code) || code != CLEAR) return {INVALID_INPUT, 0};
  while (reader.get(code)) {
    if (code == END) {
      return reader.validPadding() ? Result{OK, output} : Result{INVALID_INPUT, 0};
    }
    if (code == CLEAR || code > next || (previous == LIMIT && code >= 256)) {
      return {INVALID_INPUT, 0};
    }
    uint16_t word = code;
    size_t count = 0;
    if (code == next) {
      if (previous == LIMIT) return {INVALID_INPUT, 0};
      stack[count++] = first;
      word = previous;
    }
    while (word >= FIRST) {
      if (word >= next || count >= sizeof(stack)) return {INVALID_INPUT, 0};
      stack[count++] = suffix[word - FIRST];
      word = prefix[word - FIRST];
    }
    if (word >= 256 || count >= sizeof(stack)) return {INVALID_INPUT, 0};
    first = static_cast<uint8_t>(word);
    stack[count++] = first;
    if (output + count > MAX_DECODED_SIZE) return {INVALID_INPUT, 0};
    if (output + count > capacity) return {OUTPUT_TOO_SMALL, 0};
    while (count) dest[output++] = stack[--count];
    if (previous != LIMIT && next < LIMIT) {
      prefix[next - FIRST] = previous;
      suffix[next - FIRST] = first;
      next++;
    }
    previous = code;
  }
  return {INVALID_INPUT, 0};
}

}
