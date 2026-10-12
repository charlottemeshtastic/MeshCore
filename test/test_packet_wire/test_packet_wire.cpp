#include <gtest/gtest.h>
#include <Packet.h>
#include <vector>
#include <random>

using namespace mesh;

TEST(PacketWire, TransportCodesUseLittleEndianAndSurviveBothRouteTypes) {
  for (auto route : {ROUTE_TYPE_TRANSPORT_FLOOD, ROUTE_TYPE_TRANSPORT_DIRECT}) {
    Packet packet;
    packet.header = route | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
    packet.transport_codes[0] = 0x1234;
    packet.transport_codes[1] = 0xABCD;
    packet.payload_len = 1;
    packet.payload[0] = 0x42;
    uint8_t wire[MAX_TRANS_UNIT];
    ASSERT_EQ(7, packet.writeTo(wire));
    EXPECT_EQ(0x34, wire[1]);
    EXPECT_EQ(0x12, wire[2]);
    EXPECT_EQ(0xCD, wire[3]);
    EXPECT_EQ(0xAB, wire[4]);
    Packet decoded;
    ASSERT_TRUE(decoded.readFrom(wire, 7));
    EXPECT_EQ(packet.header, decoded.header);
    EXPECT_EQ(0x1234, decoded.transport_codes[0]);
    EXPECT_EQ(0xABCD, decoded.transport_codes[1]);
  }
}

TEST(PacketWire, TruncatedFramesNeverReadPastInput) {
  const uint8_t scoped[] = {ROUTE_TYPE_TRANSPORT_FLOOD, 1, 2, 3, 4, 2, 9, 8};
  for (size_t length = 0; length < sizeof(scoped); length++) {
    std::vector<uint8_t> exact(scoped, scoped + length);
    Packet packet;
    EXPECT_FALSE(packet.readFrom(exact.data(), exact.size())) << length;
  }
  Packet packet;
  EXPECT_TRUE(packet.readFrom(scoped, sizeof(scoped)));
  EXPECT_FALSE(packet.readFrom(nullptr, 2));
}

TEST(PacketWire, PathWidthsAndLimitsRoundTrip) {
  for (uint8_t width = 1; width <= 3; width++) {
    for (uint8_t count = 0; count < 64; count++) {
      Packet packet;
      packet.header = ROUTE_TYPE_DIRECT;
      packet.setPathHashSizeAndCount(width, count);
      memset(packet.path, 0x5A, sizeof(packet.path));
      packet.payload_len = 1;
      packet.payload[0] = 0x7F;
      uint8_t wire[MAX_TRANS_UNIT];
      size_t length = packet.writeTo(wire);
      if (width * count > MAX_PATH_SIZE) {
        EXPECT_EQ(0, length);
      } else {
        Packet decoded;
        ASSERT_TRUE(decoded.readFrom(wire, length));
        EXPECT_EQ(packet.path_len, decoded.path_len);
        EXPECT_EQ(0, memcmp(packet.path, decoded.path, width * count));
        EXPECT_EQ(0x7F, decoded.payload[0]);
      }
    }
  }
}

TEST(PacketWire, RejectsReservedVersionsPathsAndOversizeFrames) {
  Packet packet;
  const uint8_t version[] = {ROUTE_TYPE_FLOOD | (PAYLOAD_VER_2 << PH_VER_SHIFT), 0};
  const uint8_t path[] = {ROUTE_TYPE_FLOOD, 0xC0};
  EXPECT_FALSE(packet.readFrom(version, sizeof(version)));
  EXPECT_FALSE(packet.readFrom(path, sizeof(path)));
  std::vector<uint8_t> oversized(256, 0);
  EXPECT_FALSE(packet.readFrom(oversized.data(), oversized.size()));
  oversized.resize(2 + MAX_PACKET_PAYLOAD + 1);
  oversized[0] = ROUTE_TYPE_FLOOD;
  EXPECT_FALSE(packet.readFrom(oversized.data(), oversized.size()));
}

TEST(PacketWire, ZeroPayloadAndUnscopedCodesAreConsistent) {
  Packet packet;
  EXPECT_EQ(0, packet.transport_codes[0]);
  packet.header = ROUTE_TYPE_FLOOD;
  uint8_t wire[MAX_TRANS_UNIT];
  ASSERT_EQ(2, packet.writeTo(wire));
  packet.transport_codes[0] = 42;
  ASSERT_TRUE(packet.readFrom(wire, 2));
  EXPECT_EQ(0, packet.payload_len);
  EXPECT_EQ(0, packet.transport_codes[0]);
}

TEST(PacketWire, InvalidWritesLeaveDestinationUntouched) {
  Packet packet;
  uint8_t wire[MAX_TRANS_UNIT];
  memset(wire, 0xA5, sizeof(wire));
  packet.path_len = 0xC0;
  EXPECT_EQ(0, packet.writeTo(wire));
  packet.path_len = 256;
  EXPECT_EQ(0, packet.writeTo(wire));
  packet.path_len = 0;
  EXPECT_EQ(0, packet.writeTo(wire, 1));
  packet.payload_len = MAX_PACKET_PAYLOAD + 1;
  EXPECT_EQ(0, packet.writeTo(wire));
  for (auto byte : wire) EXPECT_EQ(0xA5, byte);
}

TEST(PacketWire, ArbitraryAcceptedFramesSerializeExactly) {
  std::mt19937 rng(0x5041434B);
  for (int trial = 0; trial < 3000; trial++) {
    std::vector<uint8_t> wire(rng() % 257);
    for (auto& byte : wire) byte = rng();
    Packet packet;
    if (packet.readFrom(wire.data(), wire.size())) {
      uint8_t restored[MAX_TRANS_UNIT];
      auto length = packet.writeTo(restored, sizeof(restored));
      ASSERT_EQ(wire.size(), length);
      EXPECT_EQ(wire, std::vector<uint8_t>(restored, restored + length));
    }
  }
}

TEST(PacketWire, SuppliedReplyPathChecksAvailableBytesBeforeCopy) {
  const uint8_t source[] = {0x42, 1, 2, 3, 4};
  uint8_t dest[MAX_PATH_SIZE];
  memset(dest, 0xA5, sizeof(dest));
  uint8_t metadata = 0xFF;
  for (size_t available = 0; available < sizeof(source); available++) {
    std::vector<uint8_t> exact(source, source + available);
    EXPECT_FALSE(Packet::readPath(dest, exact.data(), available, metadata));
    EXPECT_EQ(0xFF, metadata);
    for (auto byte : dest) EXPECT_EQ(0xA5, byte);
  }
  ASSERT_TRUE(Packet::readPath(dest, source, sizeof(source), metadata));
  EXPECT_EQ(0x42, metadata);
  EXPECT_EQ(0, memcmp(dest, source + 1, 4));
}

TEST(PacketWire, SuppliedZeroHopAndReservedReplyPaths) {
  uint8_t dest[MAX_PATH_SIZE], metadata = 0xFF;
  const uint8_t zero[] = {0};
  EXPECT_TRUE(Packet::readPath(dest, zero, sizeof(zero), metadata));
  EXPECT_EQ(0, metadata);
  const uint8_t reserved[] = {0xC0};
  EXPECT_FALSE(Packet::readPath(dest, reserved, sizeof(reserved), metadata));
  EXPECT_EQ(0, metadata);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
