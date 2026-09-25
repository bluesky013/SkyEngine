//
// Created on 2026/09/25.
//

#pragma once

#include <network/NetworkHost.h>

#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace sky::net::test {

    // Pumps both hosts until the predicate holds or the deadline passes. Uses small sleeps so real
    // socket backends (ENet) get time to complete handshakes and deliveries.
    inline bool PumpUntil(NetworkHost &client, NetworkHost &server, const std::function<bool()> &done,
                          int timeoutMs = 3000)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            client.Update();
            server.Update();
            if (done()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return done();
    }

    // Shared conformance suite every backend must pass. The factories create fresh backends; the suite
    // wires them through NetworkHost, connects, exercises each advertised delivery mode, and closes.
    inline void RunBackendConformance(const std::function<std::unique_ptr<INetBackend>()> &serverFactory,
                                      const std::function<std::unique_ptr<INetBackend>()> &clientFactory,
                                      const char *address)
    {
        auto serverBackend = serverFactory();
        auto clientBackend = clientFactory();
        ASSERT_TRUE(serverBackend != nullptr);
        ASSERT_TRUE(clientBackend != nullptr);
        ASSERT_TRUE(serverBackend->Init());
        ASSERT_TRUE(clientBackend->Init());

        NetworkHostConfig serverConfig;
        serverConfig.role = NetworkRole::Server;
        NetworkHost server(serverConfig);
        server.SetServer(true);
        ASSERT_TRUE(server.AttachBackend(NetworkRole::Server, serverBackend.get()));
        ASSERT_NE(server.Listen(NetworkAddress::Parse(address)), nullptr);

        NetworkHostConfig clientConfig;
        clientConfig.role = NetworkRole::Client;
        NetworkHost client(clientConfig);
        ASSERT_TRUE(client.AttachBackend(NetworkRole::Client, clientBackend.get()));

        int serverConnected = 0;
        ConnectionId serverConn = INVALID_CONNECTION_ID;
        server.SetConnectHandler([&](ConnectionId id) {
            ++serverConnected;
            serverConn = id;
        });
        int clientConnected = 0;
        ConnectionId clientConn = INVALID_CONNECTION_ID;
        client.SetConnectHandler([&](ConnectionId id) {
            ++clientConnected;
            clientConn = id;
        });

        std::vector<std::string> received;
        server.SetMessageHandler([&](ConnectionId, ChannelId, MessageSequence, std::span<const uint8_t> payload) {
            received.emplace_back(reinterpret_cast<const char *>(payload.data()), payload.size());
        });
        int serverDisconnected = 0;
        server.SetDisconnectHandler([&](ConnectionId, DisconnectReason) { ++serverDisconnected; });

        client.Connect(NetworkAddress::Parse(address));
        ASSERT_TRUE(PumpUntil(client, server, [&]() { return serverConnected == 1 && clientConnected == 1; }));
        ASSERT_EQ(serverConnected, 1);
        ASSERT_EQ(clientConnected, 1);

        auto send = [&](DeliveryMode mode, const std::string &text) {
            return client.Send(clientConn, 1,
                               std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(text.data()), text.size()),
                               mode);
        };

        // Reliable ordered, in order.
        received.clear();
        EXPECT_EQ(send(DeliveryMode::ReliableOrdered, "r0"), NetResult::Ok);
        EXPECT_EQ(send(DeliveryMode::ReliableOrdered, "r1"), NetResult::Ok);
        ASSERT_TRUE(PumpUntil(client, server, [&]() { return received.size() >= 2; }, 2000));
        ASSERT_EQ(received.size(), 2u);
        EXPECT_EQ(received[0], "r0");
        EXPECT_EQ(received[1], "r1");

        // Each advertised delivery mode delivers.
        const auto &caps = serverBackend->GetCaps();
        if (caps.Supports(DeliveryMode::Unreliable)) {
            received.clear();
            EXPECT_EQ(send(DeliveryMode::Unreliable, "u0"), NetResult::Ok);
            ASSERT_TRUE(PumpUntil(client, server, [&]() { return !received.empty(); }, 2000));
        }
        if (caps.Supports(DeliveryMode::UnreliableSequenced)) {
            received.clear();
            EXPECT_EQ(send(DeliveryMode::UnreliableSequenced, "s0"), NetResult::Ok);
            ASSERT_TRUE(PumpUntil(client, server, [&]() { return !received.empty(); }, 2000));
        }

        // Unsupported mode is rejected rather than downgraded.
        if (!caps.Supports(DeliveryMode::Unreliable)) {
            EXPECT_EQ(send(DeliveryMode::Unreliable, "x"), NetResult::UnsupportedDeliveryMode);
        }

        // Close surfaces a disconnect to the peer.
        client.Disconnect(clientConn, DisconnectReason::LocalClose);
        EXPECT_TRUE(PumpUntil(client, server, [&]() { return serverDisconnected == 1; }, 2000));
    }

} // namespace sky::net::test
