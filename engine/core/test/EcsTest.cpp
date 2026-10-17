//
// ECS container tests: EntityId layout, SparseSet, EntityRegistry.
//

#include <core/ecs/EntityRegistry.h>
#include <core/ecs/View.h>
#include <gtest/gtest.h>
#include <memory>

using namespace sky;

// test types must carry explicit tags (no fallback)
namespace test_types {
    struct EcsInt { int v = 0; };
    struct EcsFloat { float v = 0.f; };
    struct EcsVec { float x = 0.f, y = 0.f, z = 0.f; };
    struct EcsOwner {
        std::shared_ptr<int> value;
    };
} // namespace test_types

SKY_TYPE_TAG(test_types::EcsInt, "sky.test.EcsInt")
SKY_TYPE_TAG(test_types::EcsFloat, "sky.test.EcsFloat")
SKY_TYPE_TAG(test_types::EcsVec, "sky.test.EcsVec")
SKY_TYPE_TAG(test_types::EcsOwner, "sky.test.EcsOwner")

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

// ---- view ----

TEST(EcsViewTest, TwoPoolIntersection)
{
    EntityRegistry reg;
    const EntityId e1 = reg.CreateEntity();
    const EntityId e2 = reg.CreateEntity();
    const EntityId e3 = reg.CreateEntity();

    reg.Add<test_types::EcsInt>(e1, {1});
    reg.Add<test_types::EcsFloat>(e1, {1.f});
    reg.Add<test_types::EcsInt>(e2, {2});        // int only
    reg.Add<test_types::EcsFloat>(e3, {3.f});    // float only

    int hits = 0;
    reg.View<test_types::EcsInt, test_types::EcsFloat>().ForEach(
        [&](EntityId id, test_types::EcsInt &a, test_types::EcsFloat &b) {
            EXPECT_EQ(id, e1);
            EXPECT_EQ(a.v, 1);
            EXPECT_FLOAT_EQ(b.v, 1.f);
            ++hits;
        });
    EXPECT_EQ(hits, 1);
}

TEST(EcsViewTest, SmallestPoolDrives)
{
    EntityRegistry reg;
    for (uint32_t i = 0; i < 100; ++i) {
        reg.Add<test_types::EcsInt>(reg.CreateEntity(), {static_cast<int>(i)});
    }
    const EntityId e = reg.CreateEntity();
    reg.Add<test_types::EcsInt>(e, {42});
    reg.Add<test_types::EcsFloat>(e, {42.f});

    // float pool has 1 entry; view should still find the intersection
    int hits = 0;
    reg.View<test_types::EcsInt, test_types::EcsFloat>().ForEach(
        [&](EntityId, test_types::EcsInt &a, test_types::EcsFloat &) {
            EXPECT_EQ(a.v, 42);
            ++hits;
        });
    EXPECT_EQ(hits, 1);
}

TEST(EcsViewTest, ThreePoolIntersection)
{
    EntityRegistry reg;
    const EntityId e1 = reg.CreateEntity();
    const EntityId e2 = reg.CreateEntity();

    reg.Add<test_types::EcsInt>(e1, {1});
    reg.Add<test_types::EcsFloat>(e1, {1.f});
    reg.Add<test_types::EcsVec>(e1, {1.f, 2.f, 3.f});

    reg.Add<test_types::EcsInt>(e2, {2});
    reg.Add<test_types::EcsFloat>(e2, {2.f});
    // e2 lacks EcsVec

    int hits = 0;
    reg.View<test_types::EcsInt, test_types::EcsFloat, test_types::EcsVec>().ForEach(
        [&](EntityId id, test_types::EcsInt &, test_types::EcsFloat &, test_types::EcsVec &v) {
            EXPECT_EQ(id, e1);
            EXPECT_FLOAT_EQ(v.z, 3.f);
            ++hits;
        });
    EXPECT_EQ(hits, 1);
}

TEST(EcsViewTest, EmptyIntersection)
{
    EntityRegistry reg;
    const EntityId e1 = reg.CreateEntity();
    reg.Add<test_types::EcsInt>(e1, {1});
    const EntityId e2 = reg.CreateEntity();
    reg.Add<test_types::EcsFloat>(e2, {2.f});

    int hits = 0;
    reg.View<test_types::EcsInt, test_types::EcsFloat>().ForEach(
        [&](EntityId, test_types::EcsInt &, test_types::EcsFloat &) { ++hits; });
    EXPECT_EQ(hits, 0);
}

TEST(EcsViewTest, SinglePoolView)
{
    EntityRegistry reg;
    reg.Add<test_types::EcsInt>(reg.CreateEntity(), {10});
    reg.Add<test_types::EcsInt>(reg.CreateEntity(), {20});

    int sum = 0;
    reg.View<test_types::EcsInt>().ForEach(
        [&](EntityId, test_types::EcsInt &a) { sum += a.v; });
    EXPECT_EQ(sum, 30);
}

TEST(EcsRegistryTest, DestroyRemovesEveryComponentAndExistingView)
{
    EntityRegistry reg;
    const auto     first   = reg.CreateEntity();
    const auto     removed = reg.CreateEntity();
    const auto     last    = reg.CreateEntity();
    for (EntityId id : {first, removed, last}) {
        reg.Add<test_types::EcsInt>(id, {static_cast<int>(id)});
        reg.Add<test_types::EcsFloat>(id, {1});
    }
    auto               owner = std::make_shared<int>(42);
    std::weak_ptr<int> weak  = owner;
    reg.Add<test_types::EcsOwner>(removed, {std::move(owner)});
    auto view = reg.View<test_types::EcsInt, test_types::EcsFloat>();
    reg.DestroyEntity(removed);
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(reg.Pool<test_types::EcsInt>().Size(), 2u);
    EXPECT_EQ(reg.Pool<test_types::EcsFloat>().Size(), 2u);
    EXPECT_TRUE(reg.Pool<test_types::EcsOwner>().Empty());
    std::vector<EntityId> visited;
    view.ForEach([&](EntityId id, auto &a, auto &) {
        EXPECT_NE(id, removed);
        EXPECT_EQ(a.v, static_cast<int>(id));
        visited.push_back(id);
    });
    EXPECT_EQ(visited, (std::vector<EntityId>{first, last}));
}

TEST(EcsRegistryTest, DeadAndStaleUpdatesCannotResurrectOrMutateReplacement)
{
    EntityRegistry reg;
    const auto     old = reg.CreateEntity();
    reg.Add<test_types::EcsInt>(old, {1});
    reg.DestroyEntity(old);
    const auto future = MakeEntityId(GetEntityIndex(old), GetEntityGeneration(old) + 1);
    EXPECT_FALSE(reg.IsAlive(future));
    EXPECT_EQ(reg.Add<test_types::EcsInt>(old, {8}), nullptr);
    EXPECT_EQ(reg.Add<test_types::EcsInt>(future, {8}), nullptr);
    reg.DestroyEntity(old);
    reg.DestroyEntity(future);
    const auto replacement = reg.CreateEntity();
    EXPECT_EQ(replacement, future);
    reg.Add<test_types::EcsInt>(replacement, {9});
    reg.DestroyEntity(old);
    reg.Remove<test_types::EcsInt>(old);
    EXPECT_EQ(reg.Add<test_types::EcsInt>(old, {8}), nullptr);
    EXPECT_EQ(reg.Get<test_types::EcsInt>(replacement)->v, 9);
    EXPECT_EQ(reg.Pool<test_types::EcsInt>().Size(), 1u);
    EXPECT_FALSE(reg.IsAlive(INVALID_ENTITY));
}

TEST(EcsRegistryTest, ExhaustedGenerationRetiresSlotWithoutWrapping)
{
    EntityRegistry reg;
    const auto     original = reg.CreateEntity();
    auto           id       = original;
    for (uint32_t generation = 0; generation <= ECS_GENERATION_MASK; ++generation) {
        ASSERT_TRUE(reg.IsAlive(id));
        EXPECT_EQ(GetEntityGeneration(id), generation);
        EXPECT_EQ(GetEntityIndex(id), GetEntityIndex(original));
        reg.DestroyEntity(id);
        EXPECT_FALSE(reg.IsAlive(id));
        if (generation < ECS_GENERATION_MASK) {
            id = reg.CreateEntity();
        }
    }
    const auto next = reg.CreateEntity();
    EXPECT_NE(GetEntityIndex(next), GetEntityIndex(original));
    EXPECT_TRUE(reg.IsAlive(next));
    EXPECT_FALSE(reg.IsAlive(original));
    EXPECT_FALSE(reg.IsAlive(id));
}

TEST(EcsSparseSetTest, DuplicateAddUpdatesOneDenseEntryAndRejectsGenerationAlias)
{
    SparseSet<int> set;
    const auto     id = MakeEntityId(3, 0);
    set.Add(id, 1);
    set.Add(id, 2);
    EXPECT_EQ(set.Size(), 1u);
    EXPECT_EQ(*set.Get(id), 2);
    EXPECT_EQ(set.Add(MakeEntityId(3, 1), 9), nullptr);
    EXPECT_EQ(set.Add(INVALID_ENTITY, 9), nullptr);
    EXPECT_EQ(set.Size(), 1u);
    EXPECT_EQ(*set.Get(id), 2);
    set.Remove(id);
    EXPECT_TRUE(set.Empty());
}
