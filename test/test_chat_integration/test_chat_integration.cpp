#include <gtest/gtest.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/RoundTripRequest.h>
#include <random>
#include <vector>

class TestClock : public mesh::MillisecondClock, public mesh::RTCClock {
public:
  unsigned long getMillis() override { return 1000; }
  uint32_t getCurrentTime() override { return 123400; }
  void setCurrentTime(uint32_t) override {}
};

class TestRandom : public mesh::RNG {
  std::mt19937 rng;
public:
  explicit TestRandom(unsigned seed) : rng(seed) {}
  void random(uint8_t* dest, size_t length) override {
    for (size_t i = 0; i < length; i++) dest[i] = rng();
  }
};

class TestRadio : public mesh::Radio {
public:
  int last_airtime_length = 0;
  int recvRaw(uint8_t*, int) override { return 0; }
  uint32_t getEstAirtimeFor(int length) override { last_airtime_length = length; return length * 10; }
  float packetScore(float, int) override { return 0; }
  bool startSendRaw(const uint8_t*, int) override { return false; }
  bool isSendComplete() override { return true; }
  void onSendFinished() override {}
  bool isInRecvMode() const override { return true; }
};

class TestPool : public mesh::PacketManager {
  mesh::Packet slots[8];
  bool used[8]{};
public:
  std::vector<mesh::Packet*> queued;
  mesh::Packet* allocNew() override {
    for (int i = 0; i < 8; i++) {
      if (!used[i]) { used[i] = true; slots[i] = mesh::Packet(); return &slots[i]; }
    }
    return nullptr;
  }
  void free(mesh::Packet* packet) override {
    for (int i = 0; i < 8; i++) if (packet == &slots[i]) used[i] = false;
  }
  void queueOutbound(mesh::Packet* packet, uint8_t, uint32_t) override { queued.push_back(packet); }
  mesh::Packet* getNextOutbound(uint32_t) override { return take(); }
  int getOutboundCount(uint32_t) const override { return queued.size(); }
  int getOutboundTotal() const override { return queued.size(); }
  int getFreeCount() const override { int n = 0; for (bool value : used) n += !value; return n; }
  mesh::Packet* getOutboundByIdx(int index) override { return queued.at(index); }
  mesh::Packet* removeOutboundByIdx(int index) override {
    auto packet = queued.at(index); queued.erase(queued.begin() + index); return packet;
  }
  void queueInbound(mesh::Packet*, uint32_t) override {}
  mesh::Packet* getNextInbound(uint32_t) override { return nullptr; }
  mesh::Packet* take() { return queued.empty() ? nullptr : removeOutboundByIdx(0); }
};

class TestChat : public BaseChatMesh {
public:
  mutable uint8_t timeout_hops = 0;
  int requests = 0;
  std::vector<uint8_t> received;
  TestChat(TestRadio& radio, TestClock& clock, TestRandom& rng, TestPool& pool, SimpleMeshTables& tables)
    : BaseChatMesh(radio, clock, rng, clock, pool, tables) { self_id = mesh::LocalIdentity(&rng); }
  mesh::DispatcherAction inject(mesh::Packet* packet) { return mesh::Mesh::onRecvPacket(packet); }
  void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override {}
  ContactInfo* processAck(const uint8_t*) override { return nullptr; }
  void onContactPathUpdated(const ContactInfo&) override {}
  void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override {}
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override {}
  uint32_t calcFloodTimeoutMillisFor(uint32_t airtime) const override { return airtime + 1000; }
  uint32_t calcDirectTimeoutMillisFor(uint32_t airtime, uint8_t hops) const override {
    timeout_hops = hops; return airtime + 100 * hops;
  }
  void onSendTimeout() override {}
  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override {}
  uint8_t onContactRequest(const ContactInfo&, uint32_t tag, const uint8_t* data, uint8_t length, uint8_t* reply) override {
    requests++;
    memcpy(reply, &tag, 4);
    memcpy(reply + 4, data, length);
    return 4 + length;
  }
  void onContactResponse(const ContactInfo&, const uint8_t* data, uint8_t length) override {
    received.assign(data, data + length);
  }
};

class ChatIntegration : public ::testing::Test {
protected:
  TestClock clock;
  TestRandom random_a{34}, random_b{77};
  TestRadio radio_a, radio_b;
  TestPool pool_a, pool_b;
  SimpleMeshTables tables_a, tables_b;
  TestChat a{radio_a, clock, random_a, pool_a, tables_a};
  TestChat b{radio_b, clock, random_b, pool_b, tables_b};

  void SetUp() override {
    ContactInfo contact{};
    contact.type = ADV_TYPE_CHAT;
    contact.out_path_len = 1;
    contact.out_path[0] = 0x99;
    contact.id = b.self_id;
    ASSERT_TRUE(a.addContact(contact));
    contact.id = a.self_id;
    contact.out_path[0] = 0xAA;
    ASSERT_TRUE(b.addContact(contact));
  }
  ContactInfo& peer() { return *a.lookupContactByPubKey(b.self_id.pub_key, PUB_KEY_SIZE); }
  static mesh::Packet deliverAfterHops(const mesh::Packet& packet) {
    // Intermediate repeaters consume only routing metadata. Ciphertext stays intact.
    mesh::Packet delivered = packet;
    delivered.setPathHashCount(0);
    return delivered;
  }
};

TEST_F(ChatIntegration, EncryptedRequestAndReplyUseIndependentPathsWithoutReplacingContacts) {
  const uint8_t outbound[] = {0x11,0x22,0x33,0x44};
  const uint8_t inbound[] = {0x55,0x66,0x77};
  const uint8_t body[] = {1,2,3};
  uint32_t tag, timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, a.sendRoundTripRequest(peer(), outbound, 0x42, inbound, 3, body, sizeof(body), tag, timeout));
  ASSERT_EQ(1, pool_a.queued.size());
  auto request = pool_a.take();
  EXPECT_EQ(0x42, request->path_len);
  EXPECT_EQ(0, memcmp(request->path, outbound, sizeof(outbound)));
  EXPECT_EQ(request->getRawLength(), radio_a.last_airtime_length);
  EXPECT_EQ(3, a.timeout_hops);
  EXPECT_EQ(1, peer().out_path_len);
  EXPECT_EQ(0x99, peer().out_path[0]);
  auto delivered = deliverAfterHops(*request);
  b.inject(&delivered);
  ASSERT_EQ(1, b.requests);
  ASSERT_EQ(1, pool_b.queued.size());
  auto reply = pool_b.take();
  EXPECT_EQ(3, reply->path_len);
  EXPECT_EQ(0, memcmp(reply->path, inbound, sizeof(inbound)));
  auto stored = b.lookupContactByPubKey(a.self_id.pub_key, PUB_KEY_SIZE);
  EXPECT_EQ(1, stored->out_path_len);
  EXPECT_EQ(0xAA, stored->out_path[0]);
  auto returned = deliverAfterHops(*reply);
  a.inject(&returned);
  ASSERT_GE(a.received.size(), 7);
  uint32_t returned_tag;
  memcpy(&returned_tag, a.received.data(), 4);
  EXPECT_EQ(tag, returned_tag);
  EXPECT_EQ(0, memcmp(a.received.data() + 4, body, sizeof(body)));
}

TEST_F(ChatIntegration, TamperedCiphertextCannotChooseAReplyPath) {
  const uint8_t path[] = {0x55};
  const uint8_t body[] = {1};
  uint32_t tag, timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, a.sendRoundTripRequest(peer(), nullptr, 0, path, 1, body, 1, tag, timeout));
  auto delivered = deliverAfterHops(*pool_a.take());
  delivered.payload[5] ^= 1;
  b.inject(&delivered);
  EXPECT_EQ(0, b.requests);
  EXPECT_TRUE(pool_b.queued.empty());
}

TEST_F(ChatIntegration, AuthenticatedCapabilitiesBypassApplicationRequestHandlers) {
  const uint8_t body[] = {mesh::RoundTripRequest::CAPABILITY_TYPE};
  uint32_t tag, timeout;
  ASSERT_NE(MSG_SEND_FAILED, a.sendRequest(peer(), body, 1, tag, timeout));
  auto delivered = deliverAfterHops(*pool_a.take());
  b.inject(&delivered);
  EXPECT_EQ(0, b.requests);
  ASSERT_EQ(1, pool_b.queued.size());
  auto returned = deliverAfterHops(*pool_b.take());
  a.inject(&returned);
  ASSERT_GE(a.received.size(), 13);
  const uint8_t expected[] = {'C','M','F',1,1,0,0,0,0};
  EXPECT_EQ(0, memcmp(a.received.data() + 4, expected, sizeof(expected)));
}

TEST_F(ChatIntegration, ExplicitZeroHopPathsDoNotFallBackToStoredRoutes) {
  const uint8_t body[] = {1};
  uint32_t tag, timeout;
  ASSERT_EQ(MSG_SEND_SENT_DIRECT, a.sendRoundTripRequest(peer(), nullptr, 0, nullptr, 0, body, 1, tag, timeout));
  auto request = pool_a.take();
  EXPECT_EQ(0, request->getPathHashCount());
  auto delivered = *request;
  b.inject(&delivered);
  ASSERT_EQ(1, pool_b.queued.size());
  auto reply = pool_b.take();
  EXPECT_EQ(0, reply->getPathHashCount());
  EXPECT_EQ(ROUTE_TYPE_DIRECT, reply->getRouteType());
}

TEST_F(ChatIntegration, AuthenticatedUnsupportedEnvelopeIsRejectedBeforeApplicationHandling) {
  const uint8_t plaintext[] = {1,0,0,0,0xF0,2,0,1,1};
  auto packet = a.createDatagram(PAYLOAD_TYPE_REQ, peer().id, peer().getSharedSecret(a.self_id), plaintext, sizeof(plaintext));
  ASSERT_NE(nullptr, packet);
  a.sendZeroHop(packet);
  auto delivered = *pool_a.take();
  b.inject(&delivered);
  EXPECT_EQ(0, b.requests);
  EXPECT_TRUE(pool_b.queued.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
