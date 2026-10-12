#include <gtest/gtest.h>
#include <helpers/TransportCode.h>
#include <cstring>

static void fillKey(uint8_t key[16]) {
  for (int i = 0; i < 16; i++) key[i] = i;
}

TEST(TransportCode, MatchesPythonHmacSha256Vectors) {
  uint8_t key[16];
  fillKey(key);
  mesh::Packet packet;
  packet.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  packet.payload_len = 5;
  memcpy(packet.payload, "hello", 5);
  EXPECT_EQ(0x7B0F, mesh::calculateTransportCode(key, &packet));
  packet.payload_len = 0;
  EXPECT_EQ(0xF5CB, mesh::calculateTransportCode(key, &packet));
  packet.header = ROUTE_TYPE_TRANSPORT_DIRECT | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  packet.payload_len = 32;
  for (int i = 0; i < 32; i++) packet.payload[i] = i;
  EXPECT_EQ(0x4935, mesh::calculateTransportCode(key, &packet));
}

TEST(TransportCode, RemapsReservedDigestValues) {
  uint8_t key[16];
  fillKey(key);
  mesh::Packet packet;
  packet.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  packet.payload_len = 4;
  const uint8_t zero[] = {0x17, 0x22, 0x02, 0x00};
  memcpy(packet.payload, zero, 4);
  EXPECT_EQ(1, mesh::calculateTransportCode(key, &packet));
  const uint8_t maximum[] = {0x49, 0x2B, 0x00, 0x00};
  memcpy(packet.payload, maximum, 4);
  EXPECT_EQ(0xFFFE, mesh::calculateTransportCode(key, &packet));
}

TEST(TransportCode, RoutingChangesPreserveScopeWhilePayloadChangesDoNot) {
  uint8_t key[16];
  fillKey(key);
  mesh::Packet packet;
  packet.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  packet.payload_len = 5;
  memcpy(packet.payload, "hello", 5);
  auto code = mesh::calculateTransportCode(key, &packet);
  packet.header = ROUTE_TYPE_TRANSPORT_DIRECT | (PAYLOAD_TYPE_TXT_MSG << PH_TYPE_SHIFT);
  packet.setPathHashSizeAndCount(2, 3);
  memset(packet.path, 0xAB, 6);
  packet.transport_codes[0] = 42;
  EXPECT_EQ(code, mesh::calculateTransportCode(key, &packet));
  packet.payload[0] ^= 1;
  EXPECT_NE(code, mesh::calculateTransportCode(key, &packet));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
