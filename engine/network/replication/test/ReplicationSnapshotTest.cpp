//
// Created on 2026/09/25.
//

#include <network/replication/ReplicationSnapshot.h>
#include <network/replication/ReplicationTick.h>

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>

using namespace sky::net;

namespace {

    struct MockRecord : public IReplicationRecord {
        ReplicatedEntityId  entity = 0;
        ReplicationTypeId   type = 0;
        std::vector<uint8_t> state;
        FieldMask           dirty = 0;
        int                 applyCount = 0;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type; }
        void Encode(std::vector<uint8_t> &out) const override { out = state; }
        FieldMask DirtyMask() const override { return dirty; }
        void Apply(std::span<const uint8_t> data) override
        {
            state.assign(data.begin(), data.end());
            ++applyCount;
        }
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
            std::sort(records.begin(), records.end(), [](MockRecord *a, MockRecord *b) {
                return a->entity != b->entity ? a->entity < b->entity : a->type < b->type;
            });
            for (auto *record : records) {
                fn(*record);
            }
        }

        MockRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId type) override
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

TEST(ReplicationSnapshotTest, RoundTripAppliesState)
{
    MockSource server;
    server.Add(1, 100, {0xAA, 0xBB});
    server.Add(2, 100, {0xCC});

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(server, state, config, 7, message);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, message, sequence));
    EXPECT_EQ(sequence, 7u);

    ASSERT_NE(client.FindReplica(1, 100), nullptr);
    ASSERT_NE(client.FindReplica(2, 100), nullptr);
    ASSERT_EQ(client.FindReplica(1, 100)->state.size(), 2u);
    EXPECT_EQ(client.FindReplica(1, 100)->state[0], 0xAA);
    EXPECT_EQ(client.FindReplica(2, 100)->state[0], 0xCC);
}

TEST(ReplicationSnapshotTest, DeltaOmitsUnchangedRecords)
{
    MockSource server;
    server.Add(1, 100, {0x01});

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;

    std::vector<uint8_t> first;
    SnapshotCodec::BuildState(server, state, config, 1, first);

    std::vector<uint8_t> second;
    SnapshotCodec::BuildState(server, state, config, 2, second);

    SnapshotSequence sequence = 0;
    MockSource client;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, second, sequence));
    // No record emitted, so nothing is created on the client.
    EXPECT_EQ(client.FindReplica(1, 100), nullptr);
}

TEST(ReplicationSnapshotTest, ChangeReemitsRecord)
{
    MockSource server;
    auto *record = server.Add(1, 100, {0x01});

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> first;
    SnapshotCodec::BuildState(server, state, config, 1, first);

    record->state = {0x02};
    std::vector<uint8_t> second;
    SnapshotCodec::BuildState(server, state, config, 2, second);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, second, sequence));
    ASSERT_NE(client.FindReplica(1, 100), nullptr);
    EXPECT_EQ(client.FindReplica(1, 100)->state[0], 0x02);
}

TEST(ReplicationSnapshotTest, RepairAfterStaleBaselineResendsUnchanged)
{
    MockSource server;
    server.Add(1, 100, {0x01});

    ReplicationConfig config;
    config.baselineRepairTicks = 2;
    SnapshotCodec::ConnectionState state;

    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(server, state, config, 1, message);   // baseline established
    SnapshotCodec::BuildState(server, state, config, 2, message);   // ticksSinceAck = 1
    SnapshotCodec::BuildState(server, state, config, 3, message);   // ticksSinceAck = 2

    // Simulate sustained loss: never acknowledge; next build must repair with a full resend.
    std::vector<uint8_t> repair;
    SnapshotCodec::BuildState(server, state, config, 4, repair);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, repair, sequence));
    EXPECT_NE(client.FindReplica(1, 100), nullptr);
}

TEST(ReplicationSnapshotTest, AcknowledgeResetsRepairCounter)
{
    SnapshotCodec::ConnectionState state;
    state.ticksSinceAck = 10;
    SnapshotCodec::Acknowledge(state, 5);
    EXPECT_EQ(state.ticksSinceAck, 0u);
    EXPECT_EQ(state.lastAck, 5u);
}

TEST(ReplicationSnapshotTest, BudgetLimitsEntityCount)
{
    MockSource server;
    for (ReplicatedEntityId i = 1; i <= 5; ++i) {
        server.Add(i, 100, {static_cast<uint8_t>(i)});
    }

    ReplicationConfig config;
    config.maxEntitiesPerSnapshot = 2;
    SnapshotCodec::ConnectionState state;

    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(server, state, config, 1, message);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, message, sequence));
    EXPECT_LE(client.records.size(), 2u);
}

TEST(ReplicationSnapshotTest, SpawnAndDespawnEventsRoundTrip)
{
    std::vector<uint8_t> spawn;
    SnapshotCodec::BuildSpawn(9, 42, std::span<const uint8_t>(reinterpret_cast<const uint8_t *>("abc"), 3), spawn);

    ReplicationMessage type = ReplicationMessage::Ack;
    ReplicatedEntityId entity = 0;
    ReplicationTypeId typeId = 0;
    std::span<const uint8_t> payload;
    ASSERT_TRUE(SnapshotCodec::ParseEvent(spawn, type, entity, typeId, payload));
    EXPECT_EQ(type, ReplicationMessage::Spawn);
    EXPECT_EQ(entity, 9u);
    EXPECT_EQ(typeId, 42u);
    EXPECT_EQ(payload.size(), 3u);

    std::vector<uint8_t> despawn;
    SnapshotCodec::BuildDespawn(9, 42, despawn);
    ASSERT_TRUE(SnapshotCodec::ParseEvent(despawn, type, entity, typeId, payload));
    EXPECT_EQ(type, ReplicationMessage::Despawn);
    EXPECT_EQ(entity, 9u);
}

TEST(ReplicationSnapshotTest, AckRoundTrip)
{
    std::vector<uint8_t> ack;
    SnapshotCodec::BuildAck(123, ack);

    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ParseAck(ack, sequence));
    EXPECT_EQ(sequence, 123u);
}

TEST(ReplicationTickTest, FixedCadenceIndependentOfFrameRate)
{
    ReplicationTick tick(30);
    // 30 ticks per second: two 1/60 frames produce one tick, four produce two.
    EXPECT_EQ(tick.Advance(1.0 / 60.0), 0u);
    EXPECT_EQ(tick.Advance(1.0 / 60.0), 1u);
    EXPECT_EQ(tick.Advance(1.0 / 60.0), 0u);
    EXPECT_EQ(tick.Advance(1.0 / 60.0), 1u);
}

TEST(ReplicationTickTest, BoundedCatchUp)
{
    ReplicationTick tick(30, 2);
    EXPECT_EQ(tick.Advance(1.0), 2u);   // long stall is clamped to maxCatchUp
}

TEST(ReplicationSnapshotTest, AreaOfInterestExcludesEntities)
{
    struct FilteringSource : MockSource {
        ReplicatedEntityId excluded = 0;
        bool IsRelevant(const IReplicationRecord &record) const override
        {
            return record.Entity() != excluded;
        }
    };

    FilteringSource server;
    server.excluded = 2;
    server.Add(1, 100, {0x01});
    server.Add(2, 100, {0x02});
    server.Add(3, 100, {0x03});

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(server, state, config, 1, message);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, message, sequence));
    EXPECT_NE(client.FindReplica(1, 100), nullptr);
    EXPECT_EQ(client.FindReplica(2, 100), nullptr);   // excluded by AoI
    EXPECT_NE(client.FindReplica(3, 100), nullptr);
}

TEST(ReplicationSnapshotTest, PerConnectionInterestExcludesForViewer)
{
    struct ConnSource : MockSource {
        ConnectionId       cullViewer;
        ReplicatedEntityId cullEntity = 0;
        bool IsRelevantFor(const IReplicationRecord &record, ConnectionId connection) const override
        {
            return !(connection == cullViewer && record.Entity() == cullEntity);
        }
    };

    const ConnectionId viewerA{1, 1};
    const ConnectionId viewerB{2, 1};

    ConnSource server;
    server.cullViewer = viewerA;
    server.cullEntity = 2;
    server.Add(1, 100, {0x01});
    server.Add(2, 100, {0x02});
    server.Add(3, 100, {0x03});

    ReplicationConfig config;

    SnapshotCodec::ConnectionState stateA;
    std::vector<std::vector<uint8_t>> messagesA;
    SnapshotCodec::BuildStateMessages(server, stateA, config, 1, viewerA, messagesA);
    MockSource clientA;
    for (auto &message : messagesA) {
        SnapshotSequence seq = 0;
        ASSERT_TRUE(SnapshotCodec::ApplyState(clientA, message, seq));
    }
    EXPECT_NE(clientA.FindReplica(1, 100), nullptr);
    EXPECT_EQ(clientA.FindReplica(2, 100), nullptr);   // culled for viewer A

    SnapshotCodec::ConnectionState stateB;
    std::vector<std::vector<uint8_t>> messagesB;
    SnapshotCodec::BuildStateMessages(server, stateB, config, 1, viewerB, messagesB);
    MockSource clientB;
    for (auto &message : messagesB) {
        SnapshotSequence seq = 0;
        ASSERT_TRUE(SnapshotCodec::ApplyState(clientB, message, seq));
    }
    EXPECT_NE(clientB.FindReplica(2, 100), nullptr);   // visible to viewer B
}

TEST(ReplicationSnapshotTest, PriorityOrdersUnderBudget)
{
    struct PrioritySource : MockSource {
        std::unordered_map<ReplicatedEntityId, float> priorities;
        float PriorityFor(const IReplicationRecord &record, ConnectionId) const override
        {
            auto it = priorities.find(record.Entity());
            return it == priorities.end() ? 0.0f : it->second;
        }
    };

    PrioritySource server;
    server.priorities[1] = 1.0f;   // lowest
    server.priorities[2] = 3.0f;   // highest
    server.priorities[3] = 2.0f;   // middle
    server.Add(1, 100, {0x01});
    server.Add(2, 100, {0x02});
    server.Add(3, 100, {0x03});

    ReplicationConfig config;
    config.maxEntitiesPerSnapshot = 2;   // budget fits only the two highest priorities

    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(server, state, config, 1, message);

    MockSource client;
    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(client, message, sequence));
    EXPECT_EQ(client.FindReplica(1, 100), nullptr);    // lowest priority deferred
    ASSERT_NE(client.FindReplica(2, 100), nullptr);    // highest priority sent
    ASSERT_NE(client.FindReplica(3, 100), nullptr);    // second highest sent
}

TEST(ReplicationSnapshotTest, UnchangedRevisionSkipsEncoding)
{
    struct Rec : IReplicationRecord {
        ReplicatedEntityId   entity = 1;
        ReplicationTypeId    type = 100;
        std::vector<uint8_t> state{0x01};
        uint32_t             revision = 1;
        int                 *encodes = nullptr;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return type; }
        void Encode(std::vector<uint8_t> &out) const override
        {
            if (encodes != nullptr) {
                ++*encodes;
            }
            out = state;
        }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void      Apply(std::span<const uint8_t>) override {}
    };

    struct Src : IReplicationSource {
        Rec *record = nullptr;
        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override { fn(*record); }
        IReplicationRecord *CreateReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        IReplicationRecord *FindReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
    };

    int encodes = 0;
    Rec rec;
    rec.encodes = &encodes;
    Src source;
    source.record = &rec;

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> message;

    SnapshotCodec::BuildState(source, state, config, 1, message);   // repair: encode
    const int afterFirst = encodes;
    EXPECT_GE(afterFirst, 1);

    SnapshotCodec::BuildState(source, state, config, 2, message);   // unchanged revision: no encode
    EXPECT_EQ(encodes, afterFirst);

    rec.revision = 2;
    rec.state[0] = 0x09;
    SnapshotCodec::BuildState(source, state, config, 3, message);   // revision bumped: encode again
    EXPECT_EQ(encodes, afterFirst + 1);
}

TEST(ReplicationSnapshotTest, SharedCacheEncodesOnceAcrossConnections)
{
    struct CountRec : IReplicationRecord {
        ReplicatedEntityId entity = 0;
        uint32_t           revision = 1;
        int               *encodes = nullptr;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return 100; }
        void Encode(std::vector<uint8_t> &out) const override
        {
            if (encodes != nullptr) {
                ++*encodes;
            }
            out.assign(1, static_cast<uint8_t>(entity));
        }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void      Apply(std::span<const uint8_t>) override {}
    };

    struct CountSource : IReplicationSource {
        std::vector<CountRec> records;
        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override
        {
            for (auto &record : records) {
                fn(record);
            }
        }
        IReplicationRecord *CreateReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        IReplicationRecord *FindReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
    };

    int encodes = 0;
    CountSource source;
    source.records.resize(3);
    for (uint32_t i = 0; i < 3; ++i) {
        source.records[i].entity  = i + 1;
        source.records[i].encodes = &encodes;
    }

    ReplicationConfig config;
    EncodedRecordCache cache;
    std::vector<std::vector<uint8_t>> messages;

    // Four connections share one cache: each record must be encoded exactly once.
    std::vector<SnapshotCodec::ConnectionState> states(4);
    for (uint32_t c = 0; c < 4; ++c) {
        SnapshotCodec::BuildStateMessages(source, states[c], config, 1, ConnectionId{c + 1, 1}, messages, &cache);
    }
    EXPECT_EQ(encodes, 3);

    // A revision bump re-encodes only that record.
    source.records[0].revision = 2;
    SnapshotCodec::BuildStateMessages(source, states[0], config, 2, ConnectionId{1, 1}, messages, &cache);
    EXPECT_EQ(encodes, 4);
}

TEST(ReplicationSnapshotTest, CacheEraseAndClear)
{
    struct Rec2 : IReplicationRecord {
        ReplicatedEntityId entity = 0;
        uint32_t           revision = 1;
        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return 100; }
        void Encode(std::vector<uint8_t> &out) const override { out.assign(1, 0); }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void      Apply(std::span<const uint8_t>) override {}
    };
    struct Src2 : IReplicationSource {
        std::vector<Rec2> records;
        void ForEachRecord(const std::function<void(IReplicationRecord &)> &fn) override
        {
            for (auto &record : records) {
                fn(record);
            }
        }
        IReplicationRecord *CreateReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        IReplicationRecord *FindReplica(ReplicatedEntityId, ReplicationTypeId) override { return nullptr; }
    };

    Src2 source;
    source.records.resize(2);
    source.records[0].entity = 1;
    source.records[1].entity = 2;

    ReplicationConfig config;
    EncodedRecordCache cache;
    SnapshotCodec::ConnectionState state;
    std::vector<std::vector<uint8_t>> messages;
    SnapshotCodec::BuildStateMessages(source, state, config, 1, ConnectionId{1, 1}, messages, &cache);
    EXPECT_EQ(cache.Size(), 2u);

    cache.Erase(EncodedRecordCache::MakeKey(1, 100));
    EXPECT_EQ(cache.Size(), 1u);

    cache.Clear();
    EXPECT_EQ(cache.Size(), 0u);
}

TEST(ReplicationSnapshotTest, SharedCacheProducesCorrectSnapshots)
{
    struct RevRecord : public IReplicationRecord {
        ReplicatedEntityId   entity = 0;
        std::vector<uint8_t> state;
        uint32_t             revision = 1;

        ReplicatedEntityId Entity() const override { return entity; }
        ReplicationTypeId  Type() const override { return 100; }
        void Encode(std::vector<uint8_t> &out) const override { out = state; }
        FieldMask DirtyMask() const override { return 0; }
        uint32_t  Revision() const override { return revision; }
        void      Apply(std::span<const uint8_t> data) override { state.assign(data.begin(), data.end()); }
    };

    struct RevSource : public IReplicationSource {
        std::vector<std::unique_ptr<RevRecord>> storage;
        std::vector<RevRecord *>                 records;
        RevRecord *Add(ReplicatedEntityId entity, std::vector<uint8_t> state)
        {
            auto record = std::make_unique<RevRecord>();
            record->entity = entity;
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
        RevRecord *CreateReplica(ReplicatedEntityId entity, ReplicationTypeId) override
        {
            return Add(entity, {});
        }
        void DestroyReplica(ReplicatedEntityId, ReplicationTypeId) override {}
        RevRecord *FindReplica(ReplicatedEntityId entity, ReplicationTypeId) override
        {
            for (auto *record : records) {
                if (record->entity == entity) {
                    return record;
                }
            }
            return nullptr;
        }
    };

    RevSource server;
    server.Add(1, {0xAA});
    server.Add(2, {0xBB});

    ReplicationConfig config;
    EncodedRecordCache cache;
    SnapshotCodec::ConnectionState stateA;
    SnapshotCodec::ConnectionState stateB;
    std::vector<std::vector<uint8_t>> messagesA;
    std::vector<std::vector<uint8_t>> messagesB;
    SnapshotCodec::BuildStateMessages(server, stateA, config, 1, ConnectionId{1, 1}, messagesA, &cache);
    SnapshotCodec::BuildStateMessages(server, stateB, config, 1, ConnectionId{2, 1}, messagesB, &cache);

    RevSource client;
    for (auto &message : messagesA) {
        SnapshotSequence sequence = 0;
        ASSERT_TRUE(SnapshotCodec::ApplyState(client, message, sequence));
    }
    ASSERT_NE(client.FindReplica(1, 100), nullptr);
    ASSERT_NE(client.FindReplica(2, 100), nullptr);
    EXPECT_EQ(client.FindReplica(1, 100)->state[0], 0xAA);
    EXPECT_EQ(client.FindReplica(2, 100)->state[0], 0xBB);
}
