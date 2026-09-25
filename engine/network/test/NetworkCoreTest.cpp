//
// Created on 2026/09/25.
//

#include <network/NetworkAddress.h>
#include <network/NetworkBackendRegistry.h>
#include <network/NetworkHost.h>

#include "Harness.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace sky::net;
using namespace sky::net::test;

TEST(NetworkAddressTest, ParseAndFormatIPv4)
{
    NetworkAddress address = NetworkAddress::Parse("127.0.0.1:7777");
    EXPECT_EQ(address.family, NetworkAddress::Family::IPv4);
    EXPECT_EQ(address.host, "127.0.0.1");
    EXPECT_EQ(address.port, 7777);
    EXPECT_TRUE(address.IsValid());
    EXPECT_EQ(address.ToString(), "127.0.0.1:7777");
}

TEST(NetworkAddressTest, ParseAndFormatIPv6)
{
    NetworkAddress address = NetworkAddress::Parse("[::1]:8080");
    EXPECT_EQ(address.family, NetworkAddress::Family::IPv6);
    EXPECT_EQ(address.host, "::1");
    EXPECT_EQ(address.port, 8080);
    EXPECT_EQ(address.ToString(), "[::1]:8080");
}

TEST(NetworkAddressTest, ParseRejectsMalformed)
{
    EXPECT_FALSE(NetworkAddress::Parse("noport").IsValid());
    EXPECT_FALSE(NetworkAddress::Parse("host:99999").IsValid());
}

TEST(NetworkConnectionIdTest, DefaultHandleIsInvalid)
{
    EXPECT_FALSE(IsValid(INVALID_CONNECTION_ID));
    EXPECT_TRUE(IsValid(ConnectionId{1, 1}));
}

TEST(NetworkRegistryTest, RegisterResolveByRole)
{
    auto &registry = NetworkBackendRegistry::Get();
    registry.Clear();

    EXPECT_TRUE(registry.Register(NetworkRole::Client, new LoopbackBackend()));
    EXPECT_TRUE(registry.Register(NetworkRole::Server, new LoopbackBackend()));
    EXPECT_TRUE(registry.Has(NetworkRole::Client));
    EXPECT_TRUE(registry.Has(NetworkRole::Server));
    EXPECT_FALSE(registry.Has(NetworkRole::Cluster));

    EXPECT_NE(registry.Get(NetworkRole::Client), nullptr);
    EXPECT_NE(registry.Get(NetworkRole::Server), nullptr);
    EXPECT_NE(registry.Get(NetworkRole::Client), registry.Get(NetworkRole::Server));

    registry.Unregister(NetworkRole::Client);
    EXPECT_FALSE(registry.Has(NetworkRole::Client));
    EXPECT_TRUE(registry.Has(NetworkRole::Server));
    registry.Clear();
}

TEST(NetworkTransportTest, SendReceiveInOrder)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_EQ(pair.serverConnected, 1);
    ASSERT_EQ(pair.clientConnected, 1);

    std::vector<std::string> received;
    pair.server->SetMessageHandler([&](ConnectionId, ChannelId channel, MessageSequence, std::span<const uint8_t> payload) {
        EXPECT_EQ(channel, 3);
        received.emplace_back(reinterpret_cast<const char *>(payload.data()), payload.size());
    });

    for (int i = 0; i < 3; ++i) {
        const std::string text = "msg" + std::to_string(i);
        ASSERT_EQ(pair.client->Send(pair.clientConn, 3,
                                    std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(text.data()), text.size()),
                                    DeliveryMode::ReliableOrdered),
                  NetResult::Ok);
    }
    pair.Pump(4);

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], "msg0");
    EXPECT_EQ(received[1], "msg1");
    EXPECT_EQ(received[2], "msg2");
}

TEST(NetworkTransportTest, SequencedDeliveryExposesSequence)
{
    NetPair pair;
    pair.ConnectAndSettle();

    std::vector<MessageSequence> sequences;
    pair.server->SetMessageHandler([&](ConnectionId, ChannelId, MessageSequence sequence, std::span<const uint8_t>) {
        sequences.push_back(sequence);
    });

    for (int i = 0; i < 3; ++i) {
        uint8_t byte = static_cast<uint8_t>(i);
        ASSERT_EQ(pair.client->Send(pair.clientConn, 1, std::span<const uint8_t>(&byte, 1),
                                    DeliveryMode::UnreliableSequenced),
                  NetResult::Ok);
    }
    pair.Pump(4);

    ASSERT_EQ(sequences.size(), 3u);
    EXPECT_LT(sequences[0], sequences[1]);
    EXPECT_LT(sequences[1], sequences[2]);
}

TEST(NetworkTransportTest, OversizedPayloadRejected)
{
    NetPair pair;
    pair.ConnectAndSettle();

    std::vector<uint8_t> big(4000, 0);
    EXPECT_EQ(pair.client->Send(pair.clientConn, 1, big, DeliveryMode::ReliableOrdered),
              NetResult::PayloadTooLarge);
}

TEST(NetworkTransportTest, SendOnInvalidHandleFails)
{
    NetPair pair;
    pair.ConnectAndSettle();

    uint8_t byte = 0;
    EXPECT_EQ(pair.client->Send(INVALID_CONNECTION_ID, 1, std::span<const uint8_t>(&byte, 1),
                                DeliveryMode::ReliableOrdered),
              NetResult::NotConnected);
}

TEST(NetworkTransportTest, StatsReportActiveConnection)
{
    NetPair pair;
    pair.ConnectAndSettle();

    NetworkHostStats stats = pair.server->GetStats();
    EXPECT_EQ(stats.activeConnections, 1u);
    EXPECT_EQ(stats.laneCount, 1u);
}

TEST(NetworkTransportTest, UnsupportedDeliveryModeRejected)
{
    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;
    clientBackend.SetUnreliableSupported(false);

    NetworkHostConfig serverConfig;
    serverConfig.role = NetworkRole::Server;
    NetworkHost server(serverConfig);
    server.SetServer(true);
    server.AttachBackend(NetworkRole::Server, &serverBackend);
    server.Listen(NetworkAddress::Parse(LOOPBACK_ADDR));

    NetworkHostConfig clientConfig;
    clientConfig.role = NetworkRole::Client;
    NetworkHost client(clientConfig);
    client.AttachBackend(NetworkRole::Client, &clientBackend);

    ConnectionId clientConn = INVALID_CONNECTION_ID;
    client.SetConnectHandler([&](ConnectionId id) { clientConn = id; });
    client.Connect(NetworkAddress::Parse(LOOPBACK_ADDR));
    for (int i = 0; i < 10; ++i) {
        client.Update();
        server.Update();
    }

    ASSERT_TRUE(IsValid(clientConn));
    uint8_t byte = 0;
    EXPECT_EQ(client.Send(clientConn, 1, std::span<const uint8_t>(&byte, 1), DeliveryMode::Unreliable),
              NetResult::UnsupportedDeliveryMode);
    EXPECT_EQ(client.Send(clientConn, 1, std::span<const uint8_t>(&byte, 1), DeliveryMode::ReliableOrdered),
              NetResult::Ok);
}
