//
// Created on 2026/09/25.
//
// Data-driven (ECS) replication over the loopback transport: field-level delta end to end.
//

#include <network/ecs/EcsReplicationSource.h>
#include <network/replication/ReplicationHost.h>
#include <network/replication/ReplicationInterpolation.h>

#include <network/NetworkHost.h>

#include "LoopbackBackend.h"
#include "LoopbackHarness.h"

#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

namespace sky::net::test {
    struct EcsNetPos {
        float x = 0.0f;
        float y = 0.0f;
    };
} // namespace sky::net::test

SKY_TYPE_TAG(sky::net::test::EcsNetPos, "sky.net.test.EcsNetPos")

using namespace sky;
using namespace sky::net;
using namespace sky::net::test;

namespace {

    void EncodeX(const test::EcsNetPos &c, std::vector<uint8_t> &out)
    {
        out.resize(4);
        std::memcpy(out.data(), &c.x, 4);
    }
    void EncodeY(const test::EcsNetPos &c, std::vector<uint8_t> &out)
    {
        out.resize(4);
        std::memcpy(out.data(), &c.y, 4);
    }
    void ApplyX(test::EcsNetPos &c, std::span<const uint8_t> d)
    {
        if (d.size() >= 4) {
            std::memcpy(&c.x, d.data(), 4);
        }
    }
    void ApplyY(test::EcsNetPos &c, std::span<const uint8_t> d)
    {
        if (d.size() >= 4) {
            std::memcpy(&c.y, d.data(), 4);
        }
    }

} // namespace

TEST(EcsLoopbackTest, FieldDeltaEndToEnd)
{
    constexpr ReplicationTypeId TYPE = TypeId<test::EcsNetPos>();

    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;
    ASSERT_TRUE(serverBackend.Init());
    ASSERT_TRUE(clientBackend.Init());

    NetworkHostConfig serverConfig;
    serverConfig.role = NetworkRole::Server;
    NetworkHost serverHost(serverConfig);
    serverHost.SetServer(true);
    serverHost.AttachBackend(NetworkRole::Server, &serverBackend);
    serverHost.Listen(NetworkAddress::Parse("loopback:9300"));

    NetworkHostConfig clientConfig;
    clientConfig.role = NetworkRole::Client;
    NetworkHost clientHost(clientConfig);
    clientHost.AttachBackend(NetworkRole::Client, &clientBackend);

    EntityRegistry serverRegistry;
    EntityRegistry clientRegistry;
    EcsReplicationSource serverSource(serverRegistry);
    EcsReplicationSource clientSource(clientRegistry);
    serverSource.RegisterFields<test::EcsNetPos>(TYPE, {EncodeX, EncodeY}, {ApplyX, ApplyY});
    clientSource.RegisterFields<test::EcsNetPos>(TYPE, {EncodeX, EncodeY}, {ApplyX, ApplyY});

    const EntityId entity = serverRegistry.CreateEntity();
    serverRegistry.Add<test::EcsNetPos>(entity, {1.0f, 2.0f});

    ReplicationConfig config;
    ReplicationHost serverReplication(serverHost, serverSource, config);
    serverReplication.SetServer(true);
    ReplicationHost clientReplication(clientHost, clientSource, config);

    clientHost.Connect(NetworkAddress::Parse("loopback:9300"));

    auto pumpUntilReplica = [&]() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            clientHost.Update();
            serverHost.Update();
            serverReplication.Update(1.0 / 60.0);
            clientReplication.Update(1.0 / 60.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (clientRegistry.Pool<test::EcsNetPos>().Get(entity) != nullptr) {
                return true;
            }
        }
        return false;
    };

    ASSERT_TRUE(pumpUntilReplica());
    auto *client = clientRegistry.Pool<test::EcsNetPos>().Get(entity);
    ASSERT_NE(client, nullptr);
    EXPECT_FLOAT_EQ(client->x, 1.0f);
    EXPECT_FLOAT_EQ(client->y, 2.0f);

    // Change only x; the delta must carry x and preserve y on the client.
    serverRegistry.Pool<test::EcsNetPos>().Get(entity)->x = 9.0f;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        clientHost.Update();
        serverHost.Update();
        serverReplication.Update(1.0 / 60.0);
        clientReplication.Update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (clientRegistry.Pool<test::EcsNetPos>().Get(entity)->x == 9.0f) {
            break;
        }
    }
    client = clientRegistry.Pool<test::EcsNetPos>().Get(entity);
    EXPECT_FLOAT_EQ(client->x, 9.0f);
    EXPECT_FLOAT_EQ(client->y, 2.0f);
}

TEST(EcsLoopbackTest, InterpolatesReplicatedScalar)
{
    // Demonstrates the intended data-driven interpolation usage between two received states.
    SnapshotInterpolationBuffer buffer;
    const float a = 1.0f;
    const float b = 9.0f;
    uint8_t bytes[4];
    std::memcpy(bytes, &a, 4);
    buffer.Push(0.0, std::span<const uint8_t>(bytes, 4));
    std::memcpy(bytes, &b, 4);
    buffer.Push(1.0, std::span<const uint8_t>(bytes, 4));

    const double alpha = buffer.Alpha(0.5, 0.0);
    EXPECT_DOUBLE_EQ(alpha, 0.5);

    float previous = 0.0f;
    float current = 0.0f;
    std::memcpy(&previous, buffer.Previous().data(), 4);
    std::memcpy(&current, buffer.Current().data(), 4);
    const float interpolated = previous + (current - previous) * static_cast<float>(alpha);
    EXPECT_FLOAT_EQ(interpolated, 5.0f);
}

namespace {

    struct ServerPeer {
        LoopbackBackend      backend;
        EntityRegistry       registry;
        EcsReplicationSource source{registry};
        NetworkHost          host;
        ReplicationHost      replication;

        ServerPeer(const NetworkAddress &address, const ReplicationConfig &config, uint32_t dropEveryN = 0)
            : host(ServerHostConfig()),
              replication(host, source, config)
        {
            backend.Init();
            if (dropEveryN > 0) {
                backend.SetDropEveryN(dropEveryN);
            }
            host.SetServer(true);
            host.AttachBackend(NetworkRole::Server, &backend);
            source.RegisterFields<test::EcsNetPos>(TypeId<test::EcsNetPos>(), {EncodeX, EncodeY}, {ApplyX, ApplyY});
            replication.SetServer(true);
            host.Listen(address);
        }

        void Update(double dt)
        {
            host.Update();
            replication.Update(dt);
        }
        test::EcsNetPos *Get(EntityId e) { return registry.Pool<test::EcsNetPos>().Get(e); }
    };

    struct ClientPeer {
        LoopbackBackend      backend;
        EntityRegistry       registry;
        EcsReplicationSource source{registry};
        NetworkHost          host;
        ReplicationHost      replication;

        ClientPeer(const NetworkAddress &address, const ReplicationConfig &config)
            : host(ClientHostConfig()),
              replication(host, source, config)
        {
            backend.Init();
            host.AttachBackend(NetworkRole::Client, &backend);
            source.RegisterFields<test::EcsNetPos>(TypeId<test::EcsNetPos>(), {EncodeX, EncodeY}, {ApplyX, ApplyY});
            host.Connect(address);
        }

        void Update(double dt)
        {
            host.Update();
            replication.Update(dt);
        }
        test::EcsNetPos *Get(EntityId e) { return registry.Pool<test::EcsNetPos>().Get(e); }
    };

} // namespace

TEST(EcsReliabilityTest, TwoClientsConvergeIndependently)
{
    ReplicationConfig config;
    ServerPeer server(NetworkAddress::Parse("loopback:9410"), config);
    const EntityId e = server.registry.CreateEntity();
    server.registry.Add<test::EcsNetPos>(e, {1.0f, 2.0f});

    ClientPeer c1(NetworkAddress::Parse("loopback:9410"), config);
    ClientPeer c2(NetworkAddress::Parse("loopback:9410"), config);

    auto pump = [&](int iters) {
        for (int i = 0; i < iters; ++i) {
            server.Update(1.0 / 30.0);
            c1.Update(1.0 / 30.0);
            c2.Update(1.0 / 30.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    pump(40);
    ASSERT_NE(c1.Get(e), nullptr);
    ASSERT_NE(c2.Get(e), nullptr);
    EXPECT_FLOAT_EQ(c1.Get(e)->y, 2.0f);
    EXPECT_FLOAT_EQ(c2.Get(e)->y, 2.0f);

    server.registry.Pool<test::EcsNetPos>().Get(e)->x = 7.0f;
    for (int i = 0; i < 120; ++i) {
        if (c1.Get(e)->x == 7.0f && c2.Get(e)->x == 7.0f) {
            break;
        }
        pump(1);
    }
    EXPECT_FLOAT_EQ(c1.Get(e)->x, 7.0f);
    EXPECT_FLOAT_EQ(c2.Get(e)->x, 7.0f);
}

TEST(EcsReliabilityTest, LossySnapshotsConvergeViaRepair)
{
    ReplicationConfig config;
    config.baselineRepairTicks = 1;   // repair quickly after missed acks

    ServerPeer server(NetworkAddress::Parse("loopback:9420"), config, 2);   // drop every 2nd state send
    const EntityId e = server.registry.CreateEntity();
    server.registry.Add<test::EcsNetPos>(e, {1.0f, 2.0f});

    ClientPeer client(NetworkAddress::Parse("loopback:9420"), config);

    auto pump = [&](int iters) {
        for (int i = 0; i < iters; ++i) {
            server.Update(1.0 / 30.0);
            client.Update(1.0 / 30.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    pump(60);
    ASSERT_NE(client.Get(e), nullptr);

    server.registry.Pool<test::EcsNetPos>().Get(e)->x = 9.0f;
    for (int i = 0; i < 300; ++i) {
        if (client.Get(e)->x == 9.0f) {
            break;
        }
        pump(1);
    }
    EXPECT_FLOAT_EQ(client.Get(e)->x, 9.0f);
    EXPECT_FLOAT_EQ(client.Get(e)->y, 2.0f);
}

TEST(EcsReliabilityTest, SoakManyTicksNoDrift)
{
    ReplicationConfig config;
    ServerPeer server(NetworkAddress::Parse("loopback:9430"), config);

    std::vector<EntityId> moving;
    std::vector<EntityId> statics;
    for (int i = 0; i < 20; ++i) {
        const EntityId id = server.registry.CreateEntity();
        server.registry.Add<test::EcsNetPos>(id, {0.0f, 0.0f});
        moving.push_back(id);
    }
    for (int i = 0; i < 20; ++i) {
        const EntityId id = server.registry.CreateEntity();
        server.registry.Add<test::EcsNetPos>(id, {5.0f, 5.0f});
        statics.push_back(id);
    }

    ClientPeer client(NetworkAddress::Parse("loopback:9430"), config);

    float lastX = 0.0f;
    for (int t = 0; t < 120; ++t) {
        lastX = 0.01f * static_cast<float>(t + 1);
        for (auto id : moving) {
            server.registry.Pool<test::EcsNetPos>().Get(id)->x = lastX;
        }
        server.Update(1.0 / 30.0);
        client.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (int i = 0; i < 40; ++i) {
        server.Update(1.0 / 30.0);
        client.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto id : statics) {
        ASSERT_NE(client.Get(id), nullptr);
        EXPECT_FLOAT_EQ(client.Get(id)->x, 5.0f);
    }
    for (auto id : moving) {
        ASSERT_NE(client.Get(id), nullptr);
        EXPECT_FLOAT_EQ(client.Get(id)->x, lastX);
    }
}

TEST(EcsReliabilityTest, PerViewerCullingOverLoopback)
{
    ReplicationConfig config;
    ServerPeer server(NetworkAddress::Parse("loopback:9440"), config);

    ClientPeer c1(NetworkAddress::Parse("loopback:9440"), config);
    for (int i = 0; i < 60 && server.replication.ConnectionCount() < 1; ++i) {
        server.Update(1.0 / 30.0);
        c1.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(server.replication.ConnectionCount(), 1u);
    const ConnectionId c1Conn = server.replication.GetConnections()[0];

    ClientPeer c2(NetworkAddress::Parse("loopback:9440"), config);
    for (int i = 0; i < 60 && server.replication.ConnectionCount() < 2; ++i) {
        server.Update(1.0 / 30.0);
        c1.Update(1.0 / 30.0);
        c2.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(server.replication.ConnectionCount(), 2u);

    const EntityId visible = server.registry.CreateEntity();
    server.registry.Add<test::EcsNetPos>(visible, {1.0f, 1.0f});
    const EntityId hidden = server.registry.CreateEntity();
    server.registry.Add<test::EcsNetPos>(hidden, {2.0f, 2.0f});

    server.source.SetInterestFilter([c1Conn, hidden](const IReplicationRecord &record, ConnectionId connection) {
        return !(connection == c1Conn && record.Entity() == static_cast<ReplicatedEntityId>(hidden));
    });

    for (int i = 0; i < 80; ++i) {
        server.Update(1.0 / 30.0);
        c1.Update(1.0 / 30.0);
        c2.Update(1.0 / 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_NE(c1.Get(visible), nullptr);
    EXPECT_EQ(c1.Get(hidden), nullptr);    // culled for viewer 1
    EXPECT_NE(c2.Get(visible), nullptr);
    EXPECT_NE(c2.Get(hidden), nullptr);    // visible to viewer 2
}

TEST(EcsReliabilityTest, GapDetectionRecoversWithDefaultRepair)
{
    ReplicationConfig config;   // default baselineRepairTicks (60): do NOT lower it

    ServerPeer server(NetworkAddress::Parse("loopback:9450"), config, 3);   // drop 1 of 3 state sends
    const EntityId e = server.registry.CreateEntity();
    server.registry.Add<test::EcsNetPos>(e, {0.0f, 1.0f});

    ClientPeer client(NetworkAddress::Parse("loopback:9450"), config);

    auto pump = [&](int n) {
        for (int i = 0; i < n; ++i) {
            server.Update(1.0 / 30.0);
            client.Update(1.0 / 30.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    pump(40);
    ASSERT_NE(client.Get(e), nullptr);

    float last = 0.0f;
    for (int t = 0; t < 400; ++t) {
        last = static_cast<float>(t + 1);
        server.registry.Pool<test::EcsNetPos>().Get(e)->x = last;
        pump(1);
    }
    for (int i = 0; i < 300 && client.Get(e)->x != last; ++i) {
        pump(1);
    }
    EXPECT_FLOAT_EQ(client.Get(e)->x, last);
    EXPECT_FLOAT_EQ(client.Get(e)->y, 1.0f);
}
