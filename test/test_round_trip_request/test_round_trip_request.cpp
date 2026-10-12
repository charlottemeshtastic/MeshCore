#include <gtest/gtest.h>
#include <helpers/RoundTripRequest.h>
#include <random>
#include <vector>

using mesh::RoundTripRequest;

TEST(RoundTripRequest, GoldenAsymmetricReturnPathEnvelope) {
  const uint8_t path[] = {0x11, 0x22, 0x33, 0x44};
  const uint8_t body[] = {1, 0, 0};
  uint8_t encoded[184];
  auto length = RoundTripRequest::encode(encoded, sizeof(encoded), path, 0x42, body, sizeof(body));
  const std::vector<uint8_t> expected = {0xF0, 1, 0x42, 0x11, 0x22, 0x33, 0x44, 3, 1, 0, 0};
  ASSERT_EQ(expected, std::vector<uint8_t>(encoded, encoded + length));
  RoundTripRequest::Decoded decoded;
  ASSERT_TRUE(RoundTripRequest::decode(encoded, length, decoded));
  EXPECT_EQ(0x42, decoded.path_len);
  EXPECT_EQ(0, memcmp(path, decoded.path, sizeof(path)));
  EXPECT_EQ(0, memcmp(body, decoded.body, sizeof(body)));
}

TEST(RoundTripRequest, WidthsAndZeroHopRemainExplicit) {
  uint8_t path[MAX_PATH_SIZE], encoded[184];
  const uint8_t body[] = {1};
  for (auto& byte : path) byte = 0x5A;
  for (uint8_t width = 1; width <= 3; width++) {
    for (uint8_t count = 0; count < 64; count++) {
      uint8_t metadata = ((width - 1) << 6) | count;
      auto length = RoundTripRequest::encode(encoded, sizeof(encoded), path, metadata, body, sizeof(body));
      if (count * width > MAX_PATH_SIZE) {
        EXPECT_EQ(0, length);
      } else {
        ASSERT_GT(length, 0);
        RoundTripRequest::Decoded decoded;
        ASSERT_TRUE(RoundTripRequest::decode(encoded, length, decoded));
        EXPECT_EQ(metadata, decoded.path_len);
        EXPECT_EQ(0, memcmp(path, decoded.path, count * width));
      }
    }
  }
}

TEST(RoundTripRequest, TruncationDoesNotChangeOutputs) {
  const uint8_t valid[] = {0xF0, 1, 2, 0x11, 0x22, 2, 1, 0};
  RoundTripRequest::Decoded decoded{};
  decoded.path_len = 0xAA;
  decoded.body_len = 123;
  for (size_t length = 0; length < sizeof(valid); length++) {
    std::vector<uint8_t> exact(valid, valid + length);
    EXPECT_FALSE(RoundTripRequest::decode(exact.data(), exact.size(), decoded));
    EXPECT_EQ(0xAA, decoded.path_len);
    EXPECT_EQ(123, decoded.body_len);
  }
}

TEST(RoundTripRequest, AcceptsZeroCipherPaddingAndRejectsNonzeroPadding) {
  const uint8_t valid[] = {0xF0, 1, 0, 1, 1, 0, 0, 0};
  RoundTripRequest::Decoded decoded;
  ASSERT_TRUE(RoundTripRequest::decode(valid, sizeof(valid), decoded));
  EXPECT_EQ(0, decoded.path_len);
  EXPECT_EQ(1, decoded.body_len);
  auto invalid = std::vector<uint8_t>(valid, valid + sizeof(valid));
  invalid.back() = 1;
  EXPECT_FALSE(RoundTripRequest::decode(invalid.data(), invalid.size(), decoded));
}

TEST(RoundTripRequest, RejectsVersionsNestedRequestsReservedPathsAndEmptyBodies) {
  RoundTripRequest::Decoded decoded;
  for (const auto& invalid : std::vector<std::vector<uint8_t>>{
      {0xF0, 2, 0, 1, 1}, {0xF0, 1, 0xC0, 1, 1},
      {0xF0, 1, 0, 0}, {0xF0, 1, 0, 1, 0xF0}}) {
    EXPECT_FALSE(RoundTripRequest::decode(invalid.data(), invalid.size(), decoded));
  }
}

TEST(RoundTripRequest, CapacityAndPointerErrorsLeaveDestinationUntouched) {
  uint8_t encoded[184];
  memset(encoded, 0xA5, sizeof(encoded));
  const uint8_t body[] = {1};
  EXPECT_EQ(0, RoundTripRequest::encode(encoded, 4, nullptr, 0, body, 1));
  EXPECT_EQ(0, RoundTripRequest::encode(encoded, sizeof(encoded), nullptr, 1, body, 1));
  EXPECT_EQ(0, RoundTripRequest::encode(encoded, sizeof(encoded), nullptr, 0, nullptr, 1));
  EXPECT_EQ(0, RoundTripRequest::encode(encoded, sizeof(encoded), nullptr, 0, body, 0));
  for (auto byte : encoded) EXPECT_EQ(0xA5, byte);
}

TEST(RoundTripRequest, ArbitraryAcceptedEnvelopesReencodeExactlyBeforePadding) {
  std::mt19937 rng(0x524F5554);
  for (int trial = 0; trial < 3000; trial++) {
    std::vector<uint8_t> input(rng() % 185);
    for (auto& byte : input) byte = rng();
    if (input.size() > 1) { input[0] = 0xF0; input[1] = 1; }
    RoundTripRequest::Decoded decoded;
    if (RoundTripRequest::decode(input.data(), input.size(), decoded)) {
      uint8_t encoded[184];
      auto length = RoundTripRequest::encode(encoded, sizeof(encoded), decoded.path, decoded.path_len,
                                             decoded.body, decoded.body_len);
      ASSERT_GT(length, 0);
      ASSERT_LE(length, input.size());
      EXPECT_EQ(0, memcmp(encoded, input.data(), length));
    }
  }
}

static std::vector<uint8_t> manualCommand() {
  std::vector<uint8_t> frame{0xF0, 1, 1};
  for (int i = 0; i < PUB_KEY_SIZE; i++) frame.push_back(i);
  for (auto byte : {0x42, 0x11, 0x22, 0x33, 0x44, 2, 0x55, 0x66, 2, 1, 0}) frame.push_back(byte);
  return frame;
}

TEST(RoundTripRequest, CompanionCommandKeepsOutboundAndReturnPathsIndependent) {
  auto frame = manualCommand();
  RoundTripRequest::Command command;
  ASSERT_TRUE(RoundTripRequest::decodeCommand(frame.data(), frame.size(), command));
  EXPECT_EQ(0x42, command.out_path_len);
  EXPECT_EQ(0x11, command.out_path[0]);
  EXPECT_EQ(0x44, command.out_path[3]);
  EXPECT_EQ(2, command.request.path_len);
  EXPECT_EQ(0x55, command.request.path[0]);
  EXPECT_EQ(0x66, command.request.path[1]);
  EXPECT_EQ(2, command.request.body_len);
  for (int i = 0; i < PUB_KEY_SIZE; i++) EXPECT_EQ(i, command.recipient[i]);
}

TEST(RoundTripRequest, CompanionCommandRejectsTruncationMissingOptInAndTrailingBytes) {
  auto frame = manualCommand();
  RoundTripRequest::Command command{};
  command.out_path_len = 0xAA;
  for (size_t length = 0; length < frame.size(); length++) {
    std::vector<uint8_t> exact(frame.begin(), frame.begin() + length);
    EXPECT_FALSE(RoundTripRequest::decodeCommand(exact.data(), exact.size(), command));
    EXPECT_EQ(0xAA, command.out_path_len);
  }
  auto unknown = frame;
  unknown[2] = 0;
  EXPECT_FALSE(RoundTripRequest::decodeCommand(unknown.data(), unknown.size(), command));
  frame.push_back(0);
  EXPECT_FALSE(RoundTripRequest::decodeCommand(frame.data(), frame.size(), command));
}

TEST(RoundTripRequest, CapabilitiesAdvertiseOnlyImplementedManualRequestRevision) {
  uint8_t output[13];
  ASSERT_EQ(13, RoundTripRequest::writeCapabilities(output, sizeof(output), 0x12345678));
  const std::vector<uint8_t> expected = {0x78,0x56,0x34,0x12,'C','M','F',1,1,0,0,0,0};
  EXPECT_EQ(expected, std::vector<uint8_t>(output, output + sizeof(output)));
  memset(output, 0xA5, sizeof(output));
  EXPECT_EQ(0, RoundTripRequest::writeCapabilities(output, sizeof(output) - 1, 1));
  for (auto byte : output) EXPECT_EQ(0xA5, byte);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
