//
// Created on 2026/09/25.
//

#include <network/ecs/EcsReplicationSource.h>
#include <network/replication/ReplicationSnapshot.h>

#include <core/ecs/EntityRegistry.h>

#include <cstring>
#include <gtest/gtest.h>

namespace sky::net::test {
    struct EcsTransform {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct EcsPaired {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;   // deliberately unmarked
    };
} // namespace sky::net::test

SKY_TYPE_TAG(sky::net::test::EcsTransform, "sky.net.test.EcsTransform")
SKY_TYPE_TAG(sky::net::test::EcsPaired, "sky.net.test.EcsPaired")

using namespace sky;
using namespace sky::net;

namespace {

    void EncodeTransform(const test::EcsTransform &transform, std::vector<uint8_t> &out)
    {
        out.resize(sizeof(float) * 3);
        std::memcpy(out.data(), &transform.x, sizeof(float));
        std::memcpy(out.data() + sizeof(float), &transform.y, sizeof(float));
        std::memcpy(out.data() + sizeof(float) * 2, &transform.z, sizeof(float));
    }

    void ApplyTransform(test::EcsTransform &transform, std::span<const uint8_t> data)
    {
        if (data.size() < sizeof(float) * 3) {
            return;
        }
        std::memcpy(&transform.x, data.data(), sizeof(float));
        std::memcpy(&transform.y, data.data() + sizeof(float), sizeof(float));
        std::memcpy(&transform.z, data.data() + sizeof(float) * 2, sizeof(float));
    }

} // namespace

TEST(EcsReplicationTest, SnapshotCreatesAndUpdatesReplicas)
{
    constexpr ReplicationTypeId TYPE = TypeId<test::EcsTransform>();

    EntityRegistry serverRegistry;
    const EntityId e1 = serverRegistry.CreateEntity();
    const EntityId e2 = serverRegistry.CreateEntity();
    serverRegistry.Add<test::EcsTransform>(e1, {1.0f, 2.0f, 3.0f});
    serverRegistry.Add<test::EcsTransform>(e2, {4.0f, 5.0f, 6.0f});

    EcsReplicationSource serverSource(serverRegistry);
    serverSource.Register<test::EcsTransform>(TYPE, EncodeTransform, ApplyTransform);

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> message;
    SnapshotCodec::BuildState(serverSource, state, config, 1, message);

    EntityRegistry clientRegistry;
    EcsReplicationSource clientSource(clientRegistry);
    clientSource.Register<test::EcsTransform>(TYPE, EncodeTransform, ApplyTransform);

    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(clientSource, message, sequence));

    auto *t1 = clientRegistry.Pool<test::EcsTransform>().Get(e1);
    auto *t2 = clientRegistry.Pool<test::EcsTransform>().Get(e2);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);
    EXPECT_FLOAT_EQ(t1->x, 1.0f);
    EXPECT_FLOAT_EQ(t1->z, 3.0f);
    EXPECT_FLOAT_EQ(t2->z, 6.0f);
}

TEST(EcsReplicationTest, IterationOrderIsStableUnderRemove)
{
    constexpr ReplicationTypeId TYPE = TypeId<test::EcsTransform>();

    EntityRegistry registry;
    const EntityId a = registry.CreateEntity();
    const EntityId b = registry.CreateEntity();
    const EntityId c = registry.CreateEntity();
    registry.Add<test::EcsTransform>(a, {1.0f, 0.0f, 0.0f});
    registry.Add<test::EcsTransform>(b, {2.0f, 0.0f, 0.0f});
    registry.Add<test::EcsTransform>(c, {3.0f, 0.0f, 0.0f});
    registry.Remove<test::EcsTransform>(b);   // swap-remove reorders dense storage

    EcsReplicationSource source(registry);
    source.Register<test::EcsTransform>(TYPE, EncodeTransform, ApplyTransform);

    std::vector<ReplicatedEntityId> order;
    source.ForEachRecord([&](IReplicationRecord &record) { order.push_back(record.Entity()); });

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], static_cast<ReplicatedEntityId>(a));
    EXPECT_EQ(order[1], static_cast<ReplicatedEntityId>(c));
}

TEST(EcsReplicationTest, FieldDeltaOmitsUnchangedAndUnmarkedFields)
{
    using Adapter = EcsPoolAdapter<test::EcsPaired>;
    using T       = test::EcsPaired;
    constexpr ReplicationTypeId TYPE = TypeId<test::EcsPaired>();

    std::vector<Adapter::FieldEncodeFn> encoders = {
        [](const T &c, std::vector<uint8_t> &out) { out.resize(4); std::memcpy(out.data(), &c.x, 4); },
        [](const T &c, std::vector<uint8_t> &out) { out.resize(4); std::memcpy(out.data(), &c.y, 4); },
    };
    std::vector<Adapter::FieldApplyFn> appliers = {
        [](T &c, std::span<const uint8_t> d) { if (d.size() >= 4) std::memcpy(&c.x, d.data(), 4); },
        [](T &c, std::span<const uint8_t> d) { if (d.size() >= 4) std::memcpy(&c.y, d.data(), 4); },
    };

    EntityRegistry serverRegistry;
    const EntityId e = serverRegistry.CreateEntity();
    serverRegistry.Add<test::EcsPaired>(e, {1.0f, 2.0f, 99.0f});

    EcsReplicationSource serverSource(serverRegistry);
    serverSource.RegisterFields<test::EcsPaired>(TYPE, encoders, appliers);

    ReplicationConfig config;
    SnapshotCodec::ConnectionState state;
    std::vector<uint8_t> first;
    SnapshotCodec::BuildState(serverSource, state, config, 1, first);

    EntityRegistry clientRegistry;
    EcsReplicationSource clientSource(clientRegistry);
    clientSource.RegisterFields<test::EcsPaired>(TYPE, encoders, appliers);

    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(clientSource, first, sequence));
    auto *component = clientRegistry.Pool<test::EcsPaired>().Get(e);
    ASSERT_NE(component, nullptr);
    EXPECT_FLOAT_EQ(component->x, 1.0f);
    EXPECT_FLOAT_EQ(component->y, 2.0f);
    EXPECT_FLOAT_EQ(component->z, 0.0f);   // unmarked field is never replicated

    serverRegistry.Pool<test::EcsPaired>().Get(e)->x = 5.0f;

    std::vector<uint8_t> second;
    SnapshotCodec::BuildState(serverSource, state, config, 2, second);
    EXPECT_LT(second.size(), first.size());   // only one field transmitted

    ASSERT_TRUE(SnapshotCodec::ApplyState(clientSource, second, sequence));
    component = clientRegistry.Pool<test::EcsPaired>().Get(e);
    EXPECT_FLOAT_EQ(component->x, 5.0f);
    EXPECT_FLOAT_EQ(component->y, 2.0f);   // unchanged field preserved
}

TEST(EcsReplicationTest, ScalesToManyEntitiesDeterministically)
{
    constexpr ReplicationTypeId TYPE = TypeId<test::EcsTransform>();
    EntityRegistry serverRegistry;
    const uint32_t N = 1000;
    std::vector<EntityId> ids;
    ids.reserve(N);
    for (uint32_t i = 0; i < N; ++i) {
        const EntityId id = serverRegistry.CreateEntity();
        serverRegistry.Add<test::EcsTransform>(id, {static_cast<float>(i), 0.0f, 0.0f});
        ids.push_back(id);
    }

    EcsReplicationSource serverSource(serverRegistry);
    serverSource.Register<test::EcsTransform>(TYPE, EncodeTransform, ApplyTransform);

    ReplicationConfig config;
    config.maxMessageBytes         = 1u << 20;
    config.bandwidthBudgetBytes    = 1u << 30;
    config.maxEntitiesPerSnapshot  = N;

    SnapshotCodec::ConnectionState state1;
    std::vector<uint8_t> first;
    SnapshotCodec::BuildState(serverSource, state1, config, 1, first);

    // Determinism: identical source and baseline produce identical bytes.
    SnapshotCodec::ConnectionState state2;
    std::vector<uint8_t> again;
    SnapshotCodec::BuildState(serverSource, state2, config, 1, again);
    EXPECT_EQ(first, again);

    EntityRegistry clientRegistry;
    EcsReplicationSource clientSource(clientRegistry);
    clientSource.Register<test::EcsTransform>(TYPE, EncodeTransform, ApplyTransform);

    SnapshotSequence sequence = 0;
    ASSERT_TRUE(SnapshotCodec::ApplyState(clientSource, first, sequence));
    EXPECT_EQ(clientRegistry.Pool<test::EcsTransform>().Size(), N);

    auto *sample = clientRegistry.Pool<test::EcsTransform>().Get(ids[500]);
    ASSERT_NE(sample, nullptr);
    EXPECT_FLOAT_EQ(sample->x, 500.0f);
}
