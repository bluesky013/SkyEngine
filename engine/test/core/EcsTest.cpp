//
// ECS container tests: EntityId layout, SparseSet, EntityRegistry.
//

#include <core/ecs/EntityRegistry.h>
#include <gtest/gtest.h>

using namespace sky;

// test types must carry explicit tags (no fallback)
namespace test_types {
    struct EcsInt { int v = 0; };
    struct EcsFloat { float v = 0.f; };
    struct EcsVec { float x = 0.f, y = 0.f, z = 0.f; };
} // namespace test_types

SKY_TYPE_TAG(test_types::EcsInt, "sky.test.EcsInt")
SKY_TYPE_TAG(test_types::EcsFloat, "sky.test.EcsFloat")
SKY_TYPE_TAG(test_types::EcsVec, "sky.test.EcsVec")

TEST(EcsEntityIdTest, BitLayout)
{
    const EntityId id = MakeEntityId(0xABCD, 0x5A);
    EXPECT_EQ(GetEntityIndex(id), 0xABCDu);
    EXPECT_EQ(GetEntityGeneration(id), 0x5Au);
    EXPECT_TRUE(IsValidEntity(id));
    EXPECT_FALSE(IsValidEntity(INVALID_ENTITY));
}

TEST(EcsEntityIdTest, IndexMask)
{
    const EntityId id = MakeEntityId(ECS_INDEX_MASK, 0xFF);
    EXPECT_EQ(GetEntityIndex(id), ECS_INDEX_MASK);
    EXPECT_EQ(GetEntityGeneration(id), 0xFFu);
}

TEST(EcsSparseSetTest, AddGetContains)
{
    SparseSet<int> set;
    const EntityId e1 = MakeEntityId(1, 0);
    const EntityId e2 = MakeEntityId(500, 0);

    set.Add(e1, 42);
    set.Add(e2, 100);

    EXPECT_TRUE(set.Contains(e1));
    EXPECT_TRUE(set.Contains(e2));
    EXPECT_EQ(*set.Get(e1), 42);
    EXPECT_EQ(*set.Get(e2), 100);
    EXPECT_EQ(set.Size(), 2u);

    const EntityId unknown = MakeEntityId(999, 0);
    EXPECT_FALSE(set.Contains(unknown));
    EXPECT_EQ(set.Get(unknown), nullptr);
}

TEST(EcsSparseSetTest, RemoveSwapKeepsMapping)
{
    SparseSet<int> set;
    const EntityId e0 = MakeEntityId(0, 0);
    const EntityId e1 = MakeEntityId(1, 0);
    const EntityId e2 = MakeEntityId(2, 0);

    set.Add(e0, 10);
    set.Add(e1, 20);
    set.Add(e2, 30);

    set.Remove(e1); // middle removal; e2 swaps into freed slot

    EXPECT_FALSE(set.Contains(e1));
    EXPECT_TRUE(set.Contains(e0));
    EXPECT_TRUE(set.Contains(e2));
    EXPECT_EQ(*set.Get(e0), 10);
    EXPECT_EQ(*set.Get(e2), 30);
    EXPECT_EQ(set.Size(), 2u);
}

TEST(EcsSparseSetTest, RemoveLast)
{
    SparseSet<int> set;
    const EntityId e0 = MakeEntityId(0, 0);
    const EntityId e1 = MakeEntityId(1, 0);

    set.Add(e0, 10);
    set.Add(e1, 20);
    set.Remove(e1);

    EXPECT_EQ(set.Size(), 1u);
    EXPECT_TRUE(set.Contains(e0));
}

TEST(EcsSparseSetTest, StaleGenerationRejected)
{
    SparseSet<int> set;
    const EntityId oldId = MakeEntityId(3, 0);
    set.Add(oldId, 42);
    set.Remove(oldId);

    // same index, newer generation must not match stale data
    const EntityId newId = MakeEntityId(3, 1);
    EXPECT_FALSE(set.Contains(oldId));
    EXPECT_FALSE(set.Contains(newId));
}

TEST(EcsSparseSetTest, DenseIteration)
{
    SparseSet<int> set;
    for (uint32_t i = 0; i < 10; ++i) {
        set.Add(MakeEntityId(i, 0), static_cast<int>(i * 10));
    }
    set.Remove(MakeEntityId(4, 0));

    int sum = 0;
    for (uint32_t i = 0; i < set.Size(); ++i) {
        sum += set.Data(i);
    }
    EXPECT_EQ(sum, 10 + 20 + 30 + 50 + 60 + 70 + 80 + 90 + 0);
    EXPECT_EQ(set.Size(), 9u);
}

TEST(EcsRegistryTest, CreateDestroyLifecycle)
{
    EntityRegistry reg;

    const EntityId e1 = reg.CreateEntity();
    EXPECT_TRUE(reg.IsAlive(e1));

    reg.Add<test_types::EcsInt>(e1, {42});
    EXPECT_EQ(reg.Get<test_types::EcsInt>(e1)->v, 42);

    reg.DestroyEntity(e1);
    EXPECT_FALSE(reg.IsAlive(e1));
    EXPECT_EQ(reg.Get<test_types::EcsInt>(e1), nullptr); // stale generation rejected

    // index reused with bumped generation
    const EntityId e2 = reg.CreateEntity();
    EXPECT_EQ(GetEntityIndex(e2), GetEntityIndex(e1));
    EXPECT_NE(GetEntityGeneration(e2), GetEntityGeneration(e1));
    EXPECT_TRUE(reg.IsAlive(e2));
}

TEST(EcsRegistryTest, MultiplePools)
{
    EntityRegistry reg;
    const EntityId e = reg.CreateEntity();

    reg.Add<test_types::EcsInt>(e, {1});
    reg.Add<test_types::EcsFloat>(e, {2.5f});

    EXPECT_EQ(reg.Get<test_types::EcsInt>(e)->v, 1);
    EXPECT_FLOAT_EQ(reg.Get<test_types::EcsFloat>(e)->v, 2.5f);

    reg.Remove<test_types::EcsInt>(e);
    EXPECT_EQ(reg.Get<test_types::EcsInt>(e), nullptr);
    EXPECT_NE(reg.Get<test_types::EcsFloat>(e), nullptr);
}

// ---- explicit type tag ----
TEST(EcsTypeIdTest, TagDeterminesId)
{
    constexpr uint32_t expected = Fnv1a32("sky.test.EcsInt");
    EXPECT_EQ(TypeId<test_types::EcsInt>(), expected);
}

TEST(EcsTypeIdTest, DistinctTypesDistinctIds)
{
    EXPECT_NE(TypeId<test_types::EcsInt>(), TypeId<test_types::EcsFloat>());
    EXPECT_NE(TypeId<test_types::EcsInt>(), TypeId<test_types::EcsVec>());
    EXPECT_NE(TypeId<test_types::EcsFloat>(), TypeId<test_types::EcsVec>());
}

TEST(EcsTypeIdTest, StableAcrossCalls)
{
    // id depends only on the tag string; call order and repetition are irrelevant
    EXPECT_EQ(TypeId<test_types::EcsVec>(), TypeId<test_types::EcsVec>());
    EXPECT_EQ(TypeTag<test_types::EcsVec>(), "sky.test.EcsVec");
}
