//
// Created on 2026/09/25.
//

#include <network/NetworkHost.h>

#include "Harness.h"

#include <gtest/gtest.h>

using namespace sky::net;
using namespace sky::net::test;

TEST(NetworkLaneTest, ConnectionAssignedToSingleLane)
{
    NetPair pair;
    pair.ConnectAndSettle();
    ASSERT_EQ(pair.serverConnected, 1);

    EXPECT_EQ(pair.server->GetLaneCount(), 1u);
    EXPECT_EQ(pair.server->GetLaneIndex(pair.serverConn), 0u);

    NetworkHostStats stats = pair.server->GetStats();
    EXPECT_EQ(stats.laneCount, 1u);
    EXPECT_EQ(stats.lanes[0].connections, 1u);
}

TEST(NetworkLaneTest, MultipleLanesPartitionConnections)
{
    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;

    NetworkHostConfig serverConfig;
    serverConfig.role      = NetworkRole::Server;
    serverConfig.laneCount = 4;
    NetworkHost server(serverConfig);
    server.SetServer(true);
    server.AttachBackend(NetworkRole::Server, &serverBackend);
    server.Listen(NetworkAddress::Parse(LOOPBACK_ADDR));

    NetworkHostConfig clientConfig;
    clientConfig.role      = NetworkRole::Client;
    clientConfig.laneCount = 4;
    NetworkHost client(clientConfig);
    client.AttachBackend(NetworkRole::Client, &clientBackend);

    ConnectionId serverConn = INVALID_CONNECTION_ID;
    server.SetConnectHandler([&](ConnectionId id) { serverConn = id; });
    client.Connect(NetworkAddress::Parse(LOOPBACK_ADDR));
    for (int i = 0; i < 20; ++i) {
        client.Update();
        server.Update();
    }

    ASSERT_TRUE(IsValid(serverConn));
    EXPECT_EQ(server.GetLaneCount(), 4u);

    const uint32_t lane = server.GetLaneIndex(serverConn);
    EXPECT_LT(lane, 4u);

    NetworkHostStats stats = server.GetStats();
    uint32_t total = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        total += stats.lanes[i].connections;
    }
    EXPECT_EQ(total, 1u);
    EXPECT_EQ(stats.lanes[lane].connections, 1u);
}
