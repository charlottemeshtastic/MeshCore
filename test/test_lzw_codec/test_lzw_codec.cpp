#include <gtest/gtest.h>
#include <helpers/LzwCodec.h>
#include <map>
#include <random>
#include <string>
#include <vector>

using mesh::LzwCodec;

// Independent string dictionary and bit-string packing oracle.
static std::vector<uint8_t> reference(const std::vector<uint8_t>& input) {
  std::map<std::string, unsigned> dictionary;
  for (unsigned i = 0; i < 256; i++) dictionary[std::string(1, char(i))] = i;
  std::vector<unsigned> codes{256};
  std::string word;
  unsigned next = 258;
  for (auto byte : input) {
    std::string joined = word + char(byte);
    if (dictionary.count(joined)) {
      word = joined;
    } else {
      codes.push_back(dictionary.at(word));
      dictionary[joined] = next++;
      word = std::string(1, char(byte));
    }
  }
  if (!word.empty()) codes.push_back(dictionary.at(word));
  codes.push_back(257);
  std::string bits;
  for (auto code : codes) {
    for (int i = 8; i >= 0; i--) bits += (code & (1u << i)) ? '1' : '0';
  }
  while (bits.size() % 8) bits += '0';
  std::vector<uint8_t> result;
  for (size_t i = 0; i < bits.size(); i += 8) {
    result.push_back(std::stoi(bits.substr(i, 8), nullptr, 2));
  }
  return result;
}

TEST(LzwCodec, SharedGoldenVector) {
  const std::string text = "TOBEORNOTTOBEORTOBEORNOT";
  const std::vector<uint8_t> expected = {
    0x80,0x15,0x09,0xe4,0x22,0x29,0x3c,0xa4,0x4e,0x27,
    0x95,0x20,0x50,0x48,0x34,0x2e,0x0b,0x07,0x84,0xc0,0x40
  };
  uint8_t output[LzwCodec::MAX_ENCODED_SIZE];
  auto result = LzwCodec::encode(reinterpret_cast<const uint8_t*>(text.data()), text.size(), output, sizeof(output));
  ASSERT_EQ(LzwCodec::OK, result.status);
  EXPECT_EQ(expected, std::vector<uint8_t>(output, output + result.length));
}

TEST(LzwCodec, BinaryInputsMatchIndependentOracle) {
  std::mt19937 rng(0x4D455348);
  for (size_t length = 0; length <= LzwCodec::MAX_DECODED_SIZE; length++) {
    for (auto alphabet : {1, 4, 256}) {
      std::vector<uint8_t> input(length);
      for (auto& byte : input) byte = rng() % alphabet;
      uint8_t encoded[LzwCodec::MAX_ENCODED_SIZE], decoded[LzwCodec::MAX_DECODED_SIZE];
      auto result = LzwCodec::encode(input.data(), input.size(), encoded, sizeof(encoded));
      ASSERT_EQ(LzwCodec::OK, result.status);
      EXPECT_EQ(reference(input), std::vector<uint8_t>(encoded, encoded + result.length));
      auto restored = LzwCodec::decode(encoded, result.length, decoded, sizeof(decoded));
      ASSERT_EQ(LzwCodec::OK, restored.status);
      EXPECT_EQ(input, std::vector<uint8_t>(decoded, decoded + restored.length));
    }
  }
}

TEST(LzwCodec, RejectsTruncationTrailingBytesAndNonzeroPadding) {
  auto valid = reference({'A','A','A','A','A','A','A'});
  uint8_t dest[LzwCodec::MAX_DECODED_SIZE];
  for (size_t length = 0; length < valid.size(); length++) {
    EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::decode(valid.data(), length, dest, sizeof(dest)).status);
  }
  auto padded = valid;
  padded.back() |= 1;
  EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::decode(padded.data(), padded.size(), dest, sizeof(dest)).status);
  valid.push_back(0);
  EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::decode(valid.data(), valid.size(), dest, sizeof(dest)).status);
}

TEST(LzwCodec, BoundsInputOutputAndExpansion) {
  uint8_t dest[LzwCodec::MAX_ENCODED_SIZE];
  std::vector<uint8_t> input(LzwCodec::MAX_DECODED_SIZE + 1, 'A');
  EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::encode(input.data(), input.size(), dest, sizeof(dest)).status);
  auto oversized = reference(input);
  EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::decode(oversized.data(), oversized.size(), dest, sizeof(dest)).status);
  auto valid = reference({'A','B'});
  EXPECT_EQ(LzwCodec::OUTPUT_TOO_SMALL, LzwCodec::decode(valid.data(), valid.size(), dest, 1).status);
  EXPECT_EQ(LzwCodec::OUTPUT_TOO_SMALL, LzwCodec::encode(input.data(), 2, dest, 1).status);
  EXPECT_EQ(LzwCodec::INVALID_INPUT, LzwCodec::encode(nullptr, 1, dest, sizeof(dest)).status);
}

TEST(LzwCodec, RejectsArbitraryStreamsWithoutUnboundedExpansion) {
  std::mt19937 rng(42);
  uint8_t dest[LzwCodec::MAX_DECODED_SIZE];
  for (int trial = 0; trial < 2000; trial++) {
    std::vector<uint8_t> input(rng() % (LzwCodec::MAX_ENCODED_SIZE + 1));
    for (auto& byte : input) byte = rng();
    auto result = LzwCodec::decode(input.data(), input.size(), dest, sizeof(dest));
    EXPECT_LE(result.length, sizeof(dest));
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
