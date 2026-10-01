//
// Created on 2026/09/25.
//

#include <network/replication/ReplicationHost.h>

#include <network/NetworkHost.h>

#include "LoopbackBackend.h"

#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

using namespace sky::net;
using namespace sky::net::test;

namespace {

    struct MockRecord : public IReplicationRecord {
        ReplicatedEntityId   entity = 0;
        ReplicationTypeId    type = 0;
        std::vector<uint8_t> state;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type; }
        void Encode(std::vector<uint8_t> &out) const override { out = state; }
        FieldMask DirtyMask() const override { return 0; }
        void Apply(std::span<const uint8_t> data) override { state.assign(data.begin(), data.end()); }
    };

    struct MockSource : public IReplicationSource {
        std::vector<std::unique_ptr<MockRecord>> storage;
        std::vector<MockRecord *>                 records;

        MockRecord *Add(ReplicatedEntityId entity, ReplicationTypeId type, std::vector<uint8_t> state)
        {
            auto record = std::make_unique<MockRecord>();
            record->entity = entity;
            record->type   = type;
            record->state  = std::move(state);
            auto *ptr = record.get();
            records.push_back(ptr);
            storage.push_back(std::move(record));
            return ptr;
        }

        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override
        {
            for (auto *record : records) {
                fn(*record);
            }
        }

        IReplicationRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type) override
        {
            return Add(entity, type, {});
        }

        void DestroyReplica(ReplicatedEntityId entity, ReplicationTypeId type) override
        {
            for (auto it = records.begin(); it != records.end(); ++it) {
                if ((*it)->entity == entity && (*it)->type == type) {
                    records.erase(it);
                    break;
                }
            }
        }

        MockRecord *FindReplica(ReplicatedEntityId entity, ReplicationTypeId type) override
        {
            for (auto *record : records) {
                if (record->entity == entity && record->type == type) {
                    return record;
                }
            }
            return nullptr;
        }
    };

} // namespace

TEST(ReplicationHostTest, EndToEndSnapshotOverLoopback)
{
    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;
    ASSERT_TRUE(serverBackend.Init());
    ASSERT_TRUE(clientBackend.Init());

    NetworkHostConfig serverConfig;
    serverConfig.role = NetworkRole::Server;
    NetworkHost serverHost(serverConfig);
    serverHost.SetServer(true);
    serverHost.AttachBackend(NetworkRole::Server, &serverBackend);
    serverHost.Listen(NetworkAddress::Parse("loopback:9100"));

    NetworkHostConfig clientConfig;
    clientConfig.role = NetworkRole::Client;
    NetworkHost clientHost(clientConfig);
    clientHost.AttachBackend(NetworkRole::Client, &clientBackend);

    MockSource serverSource;
    MockSource clientSource;
    serverSource.Add(1, 100, {5});
    serverSource.Add(2, 100, {9});

    ReplicationConfig config;
    config.tickRateHz = 30;

    ReplicationHost serverReplication(serverHost, serverSource, config);
    serverReplication.SetServer(true);
    ReplicationHost clientReplication(clientHost, clientSource, config);

    clientHost.Connect(NetworkAddress::Parse("loopback:9100"));

    MockRecord *replica = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        clientHost.Update();
        serverHost.Update();
        serverReplication.Update(1.0 / 60.0);
        clientReplication.Update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        replica = clientSource.FindReplica(1, 100);
        if (replica != nullptr) {
            break;
        }
    }

    ASSERT_NE(replica, nullptr);
    ASSERT_EQ(replica->state.size(), 1u);
    EXPECT_EQ(replica->state[0], 5);
    EXPECT_NE(clientSource.FindReplica(2, 100), nullptr);
}

TEST(ReplicationHostTest, TickAdvancesWhileWorldPaused)
{
    NetworkHostConfig config;
    config.role = NetworkRole::Server;
    NetworkHost host(config);
    host.SetServer(true);

    MockSource source;
    ReplicationConfig replicationConfig;
    replicationConfig.tickRateHz = 30;

    ReplicationHost replication(host, source, replicationConfig);
    replication.SetServer(true);

    // The world does not advance; only real-time deltas drive the network tick.
    for (int i = 0; i < 10; ++i) {
        replication.Update(1.0 / 60.0);
    }
    EXPECT_GT(replication.TotalTicks(), 0u);
}

TEST(ReplicationHostTest, SpawnEventDeliveredDespiteSnapshotLoss)
{
    LoopbackBackend serverBackend;
    LoopbackBackend clientBackend;
    serverBackend.Init();
    clientBackend.Init();
    serverBackend.SetDropEveryN(2);   // drop every 2nd unreliable (state) send

    NetworkHostConfig serverConfig;
    serverConfig.role = NetworkRole::Server;
    NetworkHost serverHost(serverConfig);
    serverHost.SetServer(true);
    serverHost.AttachBackend(NetworkRole::Server, &serverBackend);
    serverHost.Listen(NetworkAddress::Parse("loopback:9200"));

    NetworkHostConfig clientConfig;
    clientConfig.role = NetworkRole::Client;
    NetworkHost clientHost(clientConfig);
    clientHost.AttachBackend(NetworkRole::Client, &clientBackend);

    MockSource serverSource;
    MockSource clientSource;
    serverSource.Add(42, 100, {7});

    ReplicationConfig config;
    ReplicationHost serverReplication(serverHost, serverSource, config);
    serverReplication.SetServer(true);
    ReplicationHost clientReplication(clientHost, clientSource, config);

    clientHost.Connect(NetworkAddress::Parse("loopback:9200"));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && serverReplication.ConnectionCount() == 0) {
        clientHost.Update();
        serverHost.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(serverReplication.ConnectionCount(), 1u);

    serverReplication.BroadcastSpawn(42, 100);

    MockRecord *replica = nullptr;
    while (std::chrono::steady_clock::now() < deadline) {
        clientHost.Update();
        serverHost.Update();
        serverReplication.Update(1.0 / 60.0);
        clientReplication.Update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        replica = clientSource.FindReplica(42, 100);
        if (replica != nullptr) {
            break;
        }
    }
    ASSERT_NE(replica, nullptr);
    EXPECT_EQ(replica->state.size(), 1u);
    EXPECT_EQ(replica->state[0], 7);

    // Despawn must also be delivered reliably despite ongoing snapshot loss.
    serverReplication.BroadcastDespawn(42, 100);
    bool removed = false;
    const auto despawnDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < despawnDeadline) {
        clientHost.Update();
        serverHost.Update();
        serverReplication.Update(1.0 / 60.0);
        clientReplication.Update(1.0 / 60.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (clientSource.FindReplica(42, 100) == nullptr) {
            removed = true;
            break;
        }
    }
    EXPECT_TRUE(removed);
}
