//
// Created on 2026/10/30.
//

#include <gtest/gtest.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NavigationSystem.h>

#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>

using namespace sky;
using namespace sky::ai;

namespace {
    class TestFilter final : public NaviQueryFilter {
    public:
        void SetAreaCost(AreaId, float value) override
        {
            cost = value;
        }
        void SetIncludeFlags(FilterFlags value) override
        {
            include = value;
        }
        void SetExcludeFlags(FilterFlags value) override
        {
            exclude = value;
        }
        float GetAreaCost(AreaId) const override
        {
            return cost;
        }
        FilterFlags GetIncludeFlags() const override
        {
            return include;
        }
        FilterFlags GetExcludeFlags() const override
        {
            return exclude;
        }
        float       cost    = 1;
        FilterFlags include = 0xffff, exclude = 0;
    };
    class TestMesh final : public NaviMesh {
    public:
        NaviQueryResult FindPath(const Vector3 &, const Vector3 &, const NaviQueryFilterPtr &, const NaviPathQueryParam &) const override
        {
            return NaviQueryResult::SUCCESS;
        }
        NaviQueryResult
        QueryPath(const Vector3 &, const Vector3 &, const NaviQueryFilterPtr &filter, const NaviPathQueryParam &, NaviPath &out) const override
        {
            std::unique_lock lock(mutex);
            entered = true;
            changed.notify_all();
            changed.wait(lock, [&] { return !block; });
            observedInclude = filter->GetIncludeFlags();
            observedExclude = filter->GetExcludeFlags();
            observedCost    = filter->GetAreaCost(0);
            out.result      = NaviQueryResult::SUCCESS;
            out.points.push_back({1, 2, 3});
            return out.result;
        }
        void BuildDebugGeometry(NaviDebugGeometry &) const override
        {
        }
        bool LoadData(const NaviMeshData &) override
        {
            return true;
        }
        bool ApplyBuild(const NaviMeshData &, const std::vector<NaviMeshTileCoord> &) override
        {
            return true;
        }
        bool SaveFullData(std::vector<uint8_t> &) const override
        {
            return true;
        }
        bool AddTile(const NaviMeshTilePayload &tile) override
        {
            added.push_back(tile);
            return tile.layer != failingLayer;
        }
        bool RemoveTile(const NaviMeshTileCoord &coord) override
        {
            removed.push_back(coord);
            if (failedRemovals) {
                --failedRemovals;
                return false;
            }
            return true;
        }
        bool PrepareStreaming(const NaviMeshBuildParams &) override
        {
            return true;
        }
        mutable std::mutex               mutex;
        mutable std::condition_variable  changed;
        mutable bool                     entered         = false;
        bool                             block           = false;
        uint32_t                         failingLayer    = ~uint32_t{0};
        uint32_t                         failedRemovals  = 0;
        mutable FilterFlags              observedInclude = 0, observedExclude = 0;
        mutable float                    observedCost = 0;
        std::vector<NaviMeshTilePayload> added;
        std::vector<NaviMeshTileCoord>   removed;
    };
    class TestFactory final : public NaviMeshFactory::Impl {
    public:
        CounterPtr<TestMesh> mesh = new TestMesh();
        NaviMesh            *CreateNaviMesh() override
        {
            return mesh.Get();
        }
        NaviMeshGenerator *CreateGenerator() override
        {
            return nullptr;
        }
        NaviQueryFilter *CreateQueryFilter() override
        {
            return new TestFilter();
        }
    };
    class NavigationRuntimeTest : public testing::Test {
    protected:
        void SetUp() override
        {
            NaviMeshFactory::Get()->UnRegister();
            auto *factory = new TestFactory();
            mesh          = factory->mesh;
            ASSERT_TRUE(NaviMeshFactory::Get()->Register(factory));
            world  = World::CreateWorld();
            system = new NavigationSystem();
            world->AddSubSystem(Name(INavigationSystem::NAME.data()), system);
        }
        void TearDown() override
        {
            {
                std::lock_guard lock(mesh->mutex);
                mesh->block = false;
            }
            mesh->changed.notify_all();
            TaskExecutor::Get()->WaitForAll();
            world = nullptr;
            NaviMeshFactory::Get()->UnRegister();
        }
        void Tick()
        {
            world->Tick(0);
        }
        CounterPtr<TestMesh> mesh;
        WorldPtr             world;
        NavigationSystem    *system = nullptr;
    };
} // namespace

TEST_F(NavigationRuntimeTest, OwnsManifestAndLoadsEveryLayerWithinTheCoordinateBudget)
{
    {
        NaviMeshData data;
        data.params.bounds              = {{0, -1, 0}, {20, 2, 10}};
        data.params.resolution.tileSize = 10;
        data.tiles                      = {{0, 0, 0, {1, 2}}, {0, 0, 1, {3, 4}}, {1, 0, 0, {5, 6}}};
        ASSERT_TRUE(system->SetupStreaming(data));
    }
    system->SetStreamingEnabled(true);
    system->SetLoadBudget(1);
    ASSERT_TRUE(system->SetStreamingRadii(1, 2));
    system->SetStreamingFocus({5, 0, 5});
    EXPECT_EQ(system->GetPendingLoadCount(), 1u);
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    EXPECT_EQ(system->GetPendingLoadCount(), 0u);
    ASSERT_EQ(mesh->added.size(), 2u);
    EXPECT_EQ(mesh->added[0].data, (std::vector<uint8_t>{1, 2}));
    EXPECT_EQ(mesh->added[1].layer, 1u);
    system->SetStreamingFocus({15, 0, 5});
    Tick();
    ASSERT_EQ(mesh->removed.size(), 1u);
    EXPECT_EQ(mesh->removed[0].x, 0);
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    ASSERT_EQ(mesh->added.size(), 3u);
    EXPECT_EQ(mesh->added.back().tx, 1);
}

TEST_F(NavigationRuntimeTest, FailedLayerRollsBackCoordinateAndRetriesAllLayers)
{
    NaviMeshData data;
    data.params.bounds = {{0, -1, 0}, {10, 2, 10}};
    data.tiles         = {{0, 0, 0, {1}}, {0, 0, 1, {2}}};
    ASSERT_TRUE(system->SetupStreaming(data));
    system->SetStreamingEnabled(true);
    system->SetStreamingFocus({5, 0, 5});
    mesh->failingLayer = 1;
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 0u);
    EXPECT_EQ(mesh->removed.size(), 1u);
    mesh->failingLayer = ~uint32_t{0};
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    EXPECT_EQ(mesh->added.size(), 4u);
}

TEST_F(NavigationRuntimeTest, PermanentlyFailedCoordinateDoesNotStarveNewCoordinates)
{
    NaviMeshData data;
    data.params.bounds              = {{0, -1, 0}, {20, 2, 10}};
    data.params.resolution.tileSize = 10;
    data.tiles                      = {{0, 0, 1, {1}}, {1, 0, 0, {2}}};
    ASSERT_TRUE(system->SetupStreaming(data));
    system->SetStreamingEnabled(true);
    system->SetLoadBudget(1);
    system->SetStreamingFocus({5, 0, 5});
    mesh->failingLayer = 1;
    EXPECT_EQ(system->GetPendingLoadCount(), 2u);
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 0u);
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    EXPECT_EQ(system->GetPendingLoadCount(), 1u);
    ASSERT_EQ(mesh->added.size(), 2u);
    EXPECT_EQ(mesh->added.back().tx, 1);
    Tick();
    const auto attempts = mesh->added.size();
    Tick();
    EXPECT_EQ(mesh->added.size(), attempts);
}

TEST_F(NavigationRuntimeTest, FailedRollbackIsRetriedBeforeInstallingLayersAgain)
{
    NaviMeshData data;
    data.params.bounds = {{0, -1, 0}, {10, 2, 10}};
    data.tiles         = {{0, 0, 0, {1}}, {0, 0, 1, {2}}};
    ASSERT_TRUE(system->SetupStreaming(data));
    system->SetStreamingEnabled(true);
    mesh->failingLayer   = 1;
    mesh->failedRemovals = 1;
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 0u);
    mesh->failingLayer = ~uint32_t{0};
    Tick();
    EXPECT_EQ(mesh->removed.size(), 2u);
    EXPECT_EQ(mesh->added.size(), 4u);
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
}

TEST_F(NavigationRuntimeTest, PendingRollbackIsCleanedEvenAfterFocusLeavesTheCoordinate)
{
    NaviMeshData data;
    data.params.bounds = {{0, -1, 0}, {10, 2, 10}};
    data.tiles         = {{0, 0, 0, {1}}, {0, 0, 1, {2}}};
    ASSERT_TRUE(system->SetupStreaming(data));
    system->SetStreamingEnabled(true);
    ASSERT_TRUE(system->SetStreamingRadii(1, 2));
    system->SetStreamingFocus({5, 0, 5});
    mesh->failingLayer   = 1;
    mesh->failedRemovals = 1;
    Tick();
    system->SetStreamingFocus({100, 0, 100});
    Tick();
    EXPECT_EQ(mesh->removed.size(), 2u);
    EXPECT_EQ(mesh->added.size(), 2u);
    EXPECT_EQ(system->GetPendingLoadCount(), 0u);
    mesh->failingLayer = ~uint32_t{0};
    system->SetStreamingFocus({5, 0, 5});
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
}

TEST_F(NavigationRuntimeTest, AsyncQueryFreezesFilterAndCompletedResultIgnoresLateCancellation)
{
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = true;
    }
    auto filter = NaviMeshFactory::Get()->CreateQueryFilter();
    filter->SetIncludeFlags(3);
    filter->SetExcludeFlags(4);
    filter->SetAreaCost(0, 2);
    const auto query = system->RequestPath({}, {1, 0, 1}, filter);
    ASSERT_NE(query, nullptr);
    {
        std::unique_lock lock(mesh->mutex);
        ASSERT_TRUE(mesh->changed.wait_for(lock, std::chrono::seconds(5), [&] { return mesh->entered; }));
    }
    filter->SetIncludeFlags(7);
    filter->SetExcludeFlags(8);
    filter->SetAreaCost(0, 9);
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = false;
    }
    mesh->changed.notify_all();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_EQ(mesh->observedInclude, 3u);
    EXPECT_EQ(mesh->observedExclude, 4u);
    EXPECT_FLOAT_EQ(mesh->observedCost, 2);
    ASSERT_TRUE(query->IsFinished());
    query->Cancel();
    EXPECT_FALSE(query->IsCanceled());
    EXPECT_EQ(query->GetResult(), NaviQueryResult::SUCCESS);
    EXPECT_EQ(query->GetPath().points.size(), 1u);
    EXPECT_EQ(system->GetActiveQueryCount(), 0u);
}

TEST_F(NavigationRuntimeTest, CancelledWorkerKeepsItsAdmissionSlotUntilItActuallyFinishes)
{
    system->SetQueryBudget(1);
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = true;
    }
    const auto query = system->RequestPath({}, {1, 0, 1}, NaviMeshFactory::Get()->CreateQueryFilter());
    ASSERT_NE(query, nullptr);
    {
        std::unique_lock lock(mesh->mutex);
        ASSERT_TRUE(mesh->changed.wait_for(lock, std::chrono::seconds(5), [&] { return mesh->entered; }));
    }
    system->OnNavMeshChanged();
    EXPECT_TRUE(query->IsCanceled());
    EXPECT_FALSE(query->IsFinished());
    EXPECT_EQ(system->GetActiveQueryCount(), 1u);
    EXPECT_EQ(system->RequestPath({}, {}, NaviMeshFactory::Get()->CreateQueryFilter()), nullptr);
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = false;
    }
    mesh->changed.notify_all();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_EQ(system->GetActiveQueryCount(), 0u);
    EXPECT_NE(system->RequestPath({}, {}, NaviMeshFactory::Get()->CreateQueryFilter()), nullptr);
}

TEST_F(NavigationRuntimeTest, RejectsInvalidStreamingManifestAndRadii)
{
    NaviMeshData data;
    data.params.resolution.tileSize = 0;
    EXPECT_FALSE(system->SetupStreaming(data));
    data.params.resolution.tileSize = 10;
    data.tiles                      = {{0, 0, 0, {1}}, {0, 0, 0, {2}}};
    EXPECT_FALSE(system->SetupStreaming(data));
    EXPECT_FALSE(system->SetStreamingRadii(-1, 2));
    EXPECT_FALSE(system->SetStreamingRadii(3, 2));
}

TEST_F(NavigationRuntimeTest, DetachCancelsRunningQueryAndWorkerFinishesWithOwnedState)
{
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = true;
    }
    auto query = system->RequestPath({}, {1, 0, 1}, NaviMeshFactory::Get()->CreateQueryFilter());
    ASSERT_NE(query, nullptr);
    {
        std::unique_lock lock(mesh->mutex);
        ASSERT_TRUE(mesh->changed.wait_for(lock, std::chrono::seconds(5), [&] { return mesh->entered; }));
    }
    world = nullptr;
    EXPECT_TRUE(query->IsCanceled());
    EXPECT_FALSE(query->IsFinished());
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = false;
    }
    mesh->changed.notify_all();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_TRUE(query->IsFinished());
    EXPECT_EQ(query->GetResult(), NaviQueryResult::FAILED);
    EXPECT_TRUE(query->GetPath().points.empty());
}

TEST_F(NavigationRuntimeTest, StreamingMutationCancelsTheInFlightPath)
{
    NaviMeshData data;
    data.params.bounds = {{0, -1, 0}, {10, 2, 10}};
    data.tiles         = {{0, 0, 0, {1}}};
    ASSERT_TRUE(system->SetupStreaming(data));
    system->SetStreamingEnabled(true);
    system->SetStreamingFocus({5, 0, 5});
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = true;
    }
    const auto query = system->RequestPath({}, {1, 0, 1}, NaviMeshFactory::Get()->CreateQueryFilter());
    ASSERT_NE(query, nullptr);
    {
        std::unique_lock lock(mesh->mutex);
        ASSERT_TRUE(mesh->changed.wait_for(lock, std::chrono::seconds(5), [&] { return mesh->entered; }));
    }
    Tick();
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    EXPECT_TRUE(query->IsCanceled());
    {
        std::lock_guard lock(mesh->mutex);
        mesh->block = false;
    }
    mesh->changed.notify_all();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_TRUE(query->IsFinished());
    EXPECT_EQ(query->GetResult(), NaviQueryResult::FAILED);
    EXPECT_TRUE(query->GetPath().points.empty());
}

TEST(NavigationConfigTest, ValidatesAndAppliesSettingsBeforeAttachment)
{
    NavigationSubSystemConfig config;
    NaviAgentConfig           agent;
    NaviMeshResolution        resolution;
    uint32_t                  capacity = 0;
    std::string               error;
    config.cellSize = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(config.MakeSettings(agent, resolution, capacity, error));
    config.cellSize    = .4f;
    config.cellHeight  = .2f;
    config.tileSize    = 8;
    config.agentRadius = .7f;
    config.maxTiles    = 512;
    NavigationSystem system;
    ASSERT_TRUE(system.Init(config, error));
    auto *factory = new TestFactory();
    NaviMeshFactory::Get()->UnRegister();
    ASSERT_TRUE(NaviMeshFactory::Get()->Register(factory));
    WorldPtr world = World::CreateWorld();
    static_cast<IWorldSubSystem &>(system).OnAttachToWorld(*world);
    ASSERT_NE(system.GetNaviMesh(), nullptr);
    EXPECT_FLOAT_EQ(system.GetNaviMesh()->GetResolution().cellSize, .4f);
    EXPECT_FLOAT_EQ(system.GetNaviMesh()->GetResolution().cellHeight, .2f);
    EXPECT_FLOAT_EQ(system.GetNaviMesh()->GetResolution().tileSize, 8);
    EXPECT_FLOAT_EQ(system.GetNaviMesh()->GetAgentConfig().radius, .7f);
    EXPECT_EQ(system.GetNaviMesh()->GetMaxTiles(), 512u);
    static_cast<IWorldSubSystem &>(system).OnDetachFromWorld(*world);
    NaviMeshFactory::Get()->UnRegister();
}

TEST_F(NavigationRuntimeTest, DuplicateFactoryRegistrationPreservesTheActiveOwner)
{
    auto *duplicate = new TestFactory();
    EXPECT_FALSE(NaviMeshFactory::Get()->Register(duplicate));
    EXPECT_EQ(NaviMeshFactory::Get()->CreateNaviMesh().Get(), mesh.Get());
}

TEST(NavigationConfigTest, MissingCookedAssetFailsInitializationExplicitly)
{
    NavigationSubSystemConfig config;
    config.navMeshAsset = Uuid::Create();
    NavigationSystem system;
    std::string      error;
    EXPECT_FALSE(system.Init(config, error));
    EXPECT_FALSE(error.empty());
}
