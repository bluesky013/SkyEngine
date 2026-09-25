//
// Created on 2026/09/25.
//

#include <network/NetworkHost.h>

#include "Harness.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace sky::net;
using namespace sky::net::test;

TEST(NetworkThreadingTest, OwnedThreadDeliversCallbacksOnCallerThread)
{
    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;

    NetworkHostConfig serverConfig;
    serverConfig.role = NetworkRole::Server;
    NetworkHost server(serverConfig);
    server.SetServer(true);
    server.AttachBackend(NetworkRole::Server, &serverBackend);
    server.Listen(NetworkAddress::Parse(LOOPBACK_ADDR));

    int serverConnected = 0;
    ConnectionId serverConn = INVALID_CONNECTION_ID;
    server.SetConnectHandler([&](ConnectionId id) {
        ++serverConnected;
        serverConn = id;
    });

    NetworkHostConfig clientConfig;
    clientConfig.role = NetworkRole::Client;
    NetworkHost client(clientConfig);
    client.AttachBackend(NetworkRole::Client, &clientBackend);

    const std::thread::id mainThread = std::this_thread::get_id();
    std::atomic<bool> callbackOnMain{true};
    std::atomic<int>  received{0};
    client.SetConnectHandler([&](ConnectionId) {
        if (std::this_thread::get_id() != mainThread) {
            callbackOnMain.store(false);
        }
    });
    client.SetMessageHandler([&](ConnectionId, ChannelId, MessageSequence, std::span<const uint8_t>) {
        if (std::this_thread::get_id() != mainThread) {
            callbackOnMain.store(false);
        }
        received.fetch_add(1);
    });

    client.SetThreadingModel(NetworkThreading::OwnedThread);
    client.Start();
    client.Connect(NetworkAddress::Parse(LOOPBACK_ADDR));

    int sent = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && received.load() < 2) {
        server.Update();
        client.Update();

        if (serverConnected == 1 && sent < 2) {
            uint8_t byte = static_cast<uint8_t>(sent);
            server.Send(serverConn, 1, std::span<const uint8_t>(&byte, 1), DeliveryMode::ReliableOrdered);
            ++sent;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    client.Stop();

    EXPECT_TRUE(callbackOnMain.load());
    EXPECT_EQ(serverConnected, 1);
    EXPECT_GE(received.load(), 2);
}

TEST(NetworkOverflowTest, QueueOverflowDisconnectsWithBackpressure)
{
    NetPair pair;
    pair.Init(2);
    pair.Pump(20);
    ASSERT_EQ(pair.serverConnected, 1);

    // Send more messages in one pump cycle than the server queue can hold.
    for (int i = 0; i < 6; ++i) {
        uint8_t byte = static_cast<uint8_t>(i);
        pair.client->Send(pair.clientConn, 1, std::span<const uint8_t>(&byte, 1), DeliveryMode::ReliableOrdered);
    }
    pair.server->Update();

    EXPECT_EQ(pair.serverDisconnected, 1);
    EXPECT_EQ(pair.lastServerReason, DisconnectReason::Backpressure);
    EXPECT_GE(pair.server->GetStats().messagesDropped, 1u);
}
