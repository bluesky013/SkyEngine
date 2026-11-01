//
// Created on 2026/10/30.
//

#include <framework/compression/Compressor.h>
#include <navigation/NaviMeshFactory.h>
#include <navigation/NaviMeshTypes.h>
#include <navigation/NavigationSystem.h>
#include <recast/RecastNaviMesh.h>
#include <recast/RecastNaviMeshGenerator.h>
#include <recast/RecastQueryFilter.h>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <latch>
#include <limits>

using namespace sky;
using namespace sky::ai;

namespace {
    class CopyCompressor final : public ICompressor {
    public:
        uint32_t CompressBound(uint32_t size) override
        {
            return size;
        }
        CompressResult Compress(const std::span<const uint8_t> &input, const std::span<uint8_t> &output, uint32_t) override
        {
            if (input.size() > output.size())
                return {false, 0};
            std::memcpy(output.data(), input.data(), input.size());
            return {true, static_cast<uint32_t>(input.size())};
        }
        CompressResult DeCompress(const std::span<const uint8_t> &input, const std::span<uint8_t> &output, uint32_t flags) override
        {
            return Compress(input, output, flags);
        }
    };

    class TestFactory final : public NaviMeshFactory::Impl {
    public:
        NaviMesh *CreateNaviMesh() override
        {
            return new RecastNaviMesh();
        }
        NaviMeshGenerator *CreateGenerator() override
        {
            return new RecastNaviMeshGenerator();
        }
        NaviQueryFilter *CreateQueryFilter() override
        {
            return new RecastQueryFilter();
        }
    };

    class PlainFilter : public NaviQueryFilter {
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

    private:
        float       cost    = 1.f;
        FilterFlags include = 0xffff;
        FilterFlags exclude = 0;
    };

    class BlockingFilter final : public PlainFilter {
    public:
        BlockingFilter() : releaseFuture(release.get_future().share())
        {
        }
        float GetAreaCost(AreaId area) const override
        {
            if (area == 0) {
                entered.set_value();
                releaseFuture.wait();
            }
            return PlainFilter::GetAreaCost(area);
        }
        std::future<void> GetEnteredFuture()
        {
            return entered.get_future();
        }
        void Release()
        {
            release.set_value();
        }

    private:
        mutable std::promise<void> entered;
        std::promise<void>         release;
        std::shared_future<void>   releaseFuture;
    };

    class PlaneProvider final : public INaviGeometryProvider {
    public:
        struct Plane {
            float x0, x1, z0, z1, height;
        };
        bool Collect(const AABB &, INaviGeometrySink &sink) override
        {
            for (const auto &plane : planes) {
                const std::array<Vector3, 4> vertices  = {Vector3(plane.x0, plane.height, plane.z0), Vector3(plane.x1, plane.height, plane.z0),
                                                          Vector3(plane.x1, plane.height, plane.z1), Vector3(plane.x0, plane.height, plane.z1)};
                const uint32_t               indices[] = {0, 2, 1, 0, 3, 2};
                sink.AddTriangles(vertices.data(), static_cast<uint32_t>(vertices.size()), indices, 6);
            }
            return complete;
        }
        std::vector<Plane> planes   = {{-20.f, 20.f, -10.f, 10.f, 0.f}};
        bool               complete = true;
    };

    class RecastNavigationTest : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(CompressionManager::Get()->SetEnabled(CompressionMethod::LZ4, true));
            ASSERT_TRUE(CompressionManager::Get()->Register(CompressionMethod::LZ4, new CopyCompressor()));
            ASSERT_TRUE(NaviMeshFactory::Get()->Register(new TestFactory()));
            world                                = World::CreateWorld();
            auto                      navigation = std::make_unique<NavigationSystem>();
            NavigationSubSystemConfig config;
            config.cellSize    = .25f;
            config.cellHeight  = .2f;
            config.tileSize    = 10.f;
            config.agentHeight = 1.6f;
            config.agentRadius = .3f;
            std::string error;
            ASSERT_TRUE(navigation->Init(config, error)) << error;
            system = navigation.get();
            world->AddSubSystem(Name(INavigationSystem::NAME.data()), navigation.release());
            mesh = system->GetNaviMesh();
            ASSERT_NE(mesh, nullptr);
            mesh->SetBounds(AABB(Vector3(-20.f, -2.f, -10.f), Vector3(20.f, 4.f, 10.f)));
            system->AddGeometryProvider(&provider);
        }
        void TearDown() override
        {
            TaskExecutor::Get()->WaitForAll();
            mesh  = nullptr;
            world = nullptr;
            NaviMeshFactory::Get()->UnRegister();
            CompressionManager::Get()->UnRegister(CompressionMethod::LZ4);
            EXPECT_TRUE(CompressionManager::Get()->SetEnabled(CompressionMethod::LZ4, true));
        }
        CounterPtr<NaviMeshGenerator> Build(const std::vector<NaviMeshTileCoord> &tiles = {})
        {
            auto generator = NaviMeshFactory::Get()->CreateGenerator();
            generator->SetRebuildTiles(tiles);
            generator->Setup(world);
            generator->StartAsync();
            TaskExecutor::Get()->WaitForAll();
            return generator;
        }
        NaviQueryResult Query(const Vector3 &start, const Vector3 &end, NaviPath &path)
        {
            NaviQueryFilterPtr filter = new PlainFilter();
            return mesh->QueryPath(start, end, filter, {}, path);
        }

        WorldPtr             world;
        NavigationSystem    *system = nullptr;
        CounterPtr<NaviMesh> mesh;
        PlaneProvider        provider;
    };
} // namespace

TEST_F(RecastNavigationTest, BuildsAcrossTilesAndProjectsPathWithForeignFilter)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviPath path;
    ASSERT_EQ(Query({-15.f, 2.f, 0.f}, {15.f, 2.f, 0.f}, path), NaviQueryResult::SUCCESS);
    ASSERT_GE(path.points.size(), 2u);
    EXPECT_NEAR(path.points.front().x, -15.f, .1f);
    EXPECT_NEAR(path.points.back().x, 15.f, .1f);
    EXPECT_LT(std::abs(path.points.front().y), .5f);
    EXPECT_LT(std::abs(path.points.back().y), .5f);
    EXPECT_EQ(path.flags.size(), path.points.size());
    NaviDebugGeometry debug;
    mesh->BuildDebugGeometry(debug);
    EXPECT_FALSE(debug.vertices.empty());
}

TEST_F(RecastNavigationTest, UnavailableCompressionFailsExplicitly)
{
    ASSERT_TRUE(CompressionManager::Get()->SetEnabled(CompressionMethod::LZ4, false));
    const auto generator = Build();
    EXPECT_FALSE(generator->HasSucceeded());
    EXPECT_NE(generator->GetError().find("LZ4"), std::string_view::npos);
}

TEST_F(RecastNavigationTest, DisconnectedDestinationDoesNotReportSuccess)
{
    provider.planes      = {{-20.f, -5.f, -10.f, 10.f, 0.f}, {5.f, 20.f, -10.f, 10.f, 0.f}};
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviPath path;
    EXPECT_EQ(Query({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, path), NaviQueryResult::FAILED);
    EXPECT_TRUE(path.points.empty());
    EXPECT_FALSE(path.IsValid());
    EXPECT_EQ(Query({-16.f, 0.f, 0.f}, {-8.f, 0.f, 0.f}, path), NaviQueryResult::SUCCESS);
}

TEST_F(RecastNavigationTest, IncrementalBuildRetainsUnchangedTiles)
{
    const auto initial = Build();
    ASSERT_TRUE(initial->HasSucceeded()) << initial->GetError();
    const auto incremental = Build({{0, 0}});
    ASSERT_TRUE(incremental->HasSucceeded()) << incremental->GetError();
    NaviPath path;
    EXPECT_EQ(Query({5.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, path), NaviQueryResult::SUCCESS);
    EXPECT_EQ(Query({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, path), NaviQueryResult::SUCCESS);
}

TEST_F(RecastNavigationTest, TiledAndFullPayloadRoundTripsPreserveReachability)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviMeshData data;
    generator->CollectTiles(data);
    ASSERT_FALSE(data.tiles.empty());
    EXPECT_EQ(data.tiles.front().tx, 0);
    EXPECT_EQ(data.tiles.front().ty, 0);
    CounterPtr<NaviMesh> loaded = new RecastNaviMesh();
    ASSERT_TRUE(loaded->LoadData(data));
    NaviQueryFilterPtr filter = new PlainFilter();
    NaviPath           path;
    EXPECT_EQ(loaded->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path), NaviQueryResult::SUCCESS);
    data.mode = NaviMeshExportMode::Full;
    ASSERT_TRUE(mesh->SaveFullData(data.fullData));
    ASSERT_TRUE(loaded->LoadData(data));
    EXPECT_EQ(loaded->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path), NaviQueryResult::SUCCESS);
}

TEST_F(RecastNavigationTest, CorruptTileFailsLoadingAndCanBeReloaded)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviMeshData good;
    generator->CollectTiles(good);
    auto corrupt = good;
    ASSERT_FALSE(corrupt.tiles.empty());
    corrupt.tiles.front().data.assign(8, 0xff);
    CounterPtr<NaviMesh> loaded = new RecastNaviMesh();
    EXPECT_FALSE(loaded->LoadData(corrupt));
    EXPECT_TRUE(loaded->LoadData(good));
    NaviQueryFilterPtr filter = new PlainFilter();
    NaviPath           path;
    EXPECT_EQ(loaded->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path), NaviQueryResult::SUCCESS);
    EXPECT_FALSE(loaded->AddTile(corrupt.tiles.front()));
}

TEST_F(RecastNavigationTest, IncompleteProviderAndInvalidSetupFailExplicitly)
{
    provider.complete     = false;
    const auto incomplete = Build();
    EXPECT_FALSE(incomplete->HasSucceeded());
    EXPECT_FALSE(incomplete->GetError().empty());
    provider.complete = true;
    mesh->SetResolution({0.f, .2f, 10.f});
    const auto invalid = Build();
    EXPECT_FALSE(invalid->HasSucceeded());
    EXPECT_FALSE(invalid->GetError().empty());
    CounterPtr<NaviMeshGenerator> noWorld = new RecastNaviMeshGenerator();
    noWorld->Setup({});
    noWorld->StartAsync();
    TaskExecutor::Get()->WaitForAll();
    EXPECT_FALSE(noWorld->HasSucceeded());
    EXPECT_FALSE(noWorld->GetError().empty());
}

TEST_F(RecastNavigationTest, StreamingOwnsPayloadAndUsesBoundsOrigin)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    {
        NaviMeshData data;
        generator->CollectTiles(data);
        ASSERT_TRUE(system->SetupStreaming(data));
    }
    ASSERT_TRUE(system->SetStreamingRadii(12.f, 20.f));
    system->SetStreamingFocus({-15.f, 0.f, -5.f});
    system->SetStreamingEnabled(true);
    static_cast<IWorldSubSystem *>(system)->Tick(0.f);
    EXPECT_GT(system->GetLoadedTileCount(), 0u);
    NaviPath path;
    EXPECT_EQ(Query({-17.f, 0.f, -5.f}, {-13.f, 0.f, -5.f}, path), NaviQueryResult::SUCCESS);
}

TEST_F(RecastNavigationTest, InvalidFilterCostsAndFlagsReturnFailure)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviQueryFilterPtr filter = new PlainFilter();
    NaviPath           path;
    filter->SetIncludeFlags(0);
    EXPECT_EQ(mesh->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path), NaviQueryResult::FAILED);
    filter->SetIncludeFlags(0xffff);
    filter->SetAreaCost(0, std::numeric_limits<float>::quiet_NaN());
    EXPECT_EQ(mesh->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path), NaviQueryResult::FAILED);
}

TEST_F(RecastNavigationTest, RemovingMissingCoordinatesIsIdempotentOnAnInitializedMesh)
{
    CounterPtr<NaviMesh> uninitialized = new RecastNaviMesh();
    EXPECT_FALSE(uninitialized->RemoveTile({0, 0}));
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    EXPECT_TRUE(mesh->RemoveTile({99, 99}));
    EXPECT_TRUE(mesh->RemoveTile({99, 99}));
    EXPECT_TRUE(mesh->RemoveTile({0, 0}));
    EXPECT_TRUE(mesh->RemoveTile({0, 0}));
}

TEST_F(RecastNavigationTest, StreamingRecoversAfterTheFirstLayerFailsWithoutLeavingATile)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviMeshData data;
    generator->CollectTiles(data);
    ASSERT_FALSE(data.tiles.empty());
    const auto &tile = data.tiles.front();
    const float x    = data.params.bounds.min.x + (static_cast<float>(tile.tx) + .5f) * data.params.resolution.tileSize;
    const float z    = data.params.bounds.min.z + (static_cast<float>(tile.ty) + .5f) * data.params.resolution.tileSize;
    ASSERT_TRUE(system->SetupStreaming(data));
    ASSERT_TRUE(system->SetStreamingRadii(1.f, 2.f));
    system->SetLoadBudget(1);
    system->SetStreamingFocus({x, 0.f, z});
    system->SetStreamingEnabled(true);
    ASSERT_TRUE(CompressionManager::Get()->SetEnabled(CompressionMethod::LZ4, false));
    static_cast<IWorldSubSystem *>(system)->Tick(0.f);
    EXPECT_EQ(system->GetLoadedTileCount(), 0u);
    EXPECT_EQ(system->GetPendingLoadCount(), 1u);
    ASSERT_TRUE(CompressionManager::Get()->SetEnabled(CompressionMethod::LZ4, true));
    static_cast<IWorldSubSystem *>(system)->Tick(0.f);
    EXPECT_EQ(system->GetLoadedTileCount(), 1u);
    EXPECT_EQ(system->GetPendingLoadCount(), 0u);
    NaviPath path;
    EXPECT_EQ(Query({x - 1.f, 0.f, z}, {x + 1.f, 0.f, z}, path), NaviQueryResult::SUCCESS);
}

TEST_F(RecastNavigationTest, ASecondReaderCompletesWhileTheFirstReaderIsHeld)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    CounterPtr<BlockingFilter> filter       = new BlockingFilter();
    auto                       entered      = filter->GetEnteredFuture();
    auto                       first        = std::async(std::launch::async, [mesh = mesh, filter] {
        NaviPath path;
        return mesh->QueryPath({-15.f, 0.f, 0.f}, {15.f, 0.f, 0.f}, filter, {}, path);
    });
    const auto                 firstEntered = entered.wait_for(std::chrono::seconds(5));
    if (firstEntered != std::future_status::ready) {
        filter->Release();
        first.wait();
        FAIL() << "The first reader did not reach its held read section";
        return;
    }

    auto second = std::async(std::launch::async, [mesh = mesh] {
        NaviQueryFilterPtr otherFilter = new PlainFilter();
        NaviPath           path;
        return mesh->QueryPath({15.f, 0.f, 5.f}, {-15.f, 0.f, 5.f}, otherFilter, {}, path);
    });
    // This timeout bounds a deadlock; completion is required before releasing the first reader.
    const bool secondFinished = second.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    filter->Release();
    EXPECT_EQ(first.get(), NaviQueryResult::SUCCESS);
    EXPECT_EQ(second.get(), NaviQueryResult::SUCCESS);
    EXPECT_TRUE(secondFinished);
}

TEST_F(RecastNavigationTest, ConcurrentQueriesKeepTheirSearchResultsIndependent)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    std::latch                       start(1);
    std::array<std::future<bool>, 4> readers;
    for (size_t worker = 0; worker < readers.size(); ++worker) {
        readers[worker] = std::async(std::launch::async, [mesh = mesh, &start, worker] {
            NaviQueryFilterPtr filter = new PlainFilter();
            const float        z      = -6.f + static_cast<float>(worker) * 4.f;
            start.wait();
            for (uint32_t repeat = 0; repeat < 64; ++repeat) {
                const float direction = repeat % 2 == 0 ? 1.f : -1.f;
                NaviPath    path;
                if (mesh->QueryPath({-15.f * direction, 0.f, z}, {15.f * direction, 0.f, z}, filter, {}, path) != NaviQueryResult::SUCCESS ||
                    path.points.size() < 2 || path.flags.size() != path.points.size() || std::abs(path.points.front().x + 15.f * direction) > .1f ||
                    std::abs(path.points.back().x - 15.f * direction) > .1f || std::abs(path.points.front().z - z) > .1f ||
                    std::abs(path.points.back().z - z) > .1f) {
                    return false;
                }
            }
            return true;
        });
    }
    start.count_down();
    for (auto &reader : readers) {
        EXPECT_TRUE(reader.get());
    }
}

TEST_F(RecastNavigationTest, ConcurrentQueriesSurviveUnrelatedTileReplacement)
{
    const auto generator = Build();
    ASSERT_TRUE(generator->HasSucceeded()) << generator->GetError();
    NaviMeshData data;
    generator->CollectTiles(data);
    std::vector<NaviMeshTilePayload> layers;
    for (const auto &tile : data.tiles) {
        if (tile.tx == 0 && tile.ty == 0) {
            layers.push_back(tile);
        }
    }
    ASSERT_FALSE(layers.empty());
    std::latch                       start(1);
    std::array<std::future<bool>, 4> readers;
    for (auto &reader : readers) {
        reader = std::async(std::launch::async, [mesh = mesh, &start] {
            NaviQueryFilterPtr filter = new PlainFilter();
            start.wait();
            for (uint32_t repeat = 0; repeat < 64; ++repeat) {
                NaviPath path;
                if (mesh->QueryPath({5.f, 0.f, 5.f}, {15.f, 0.f, 5.f}, filter, {}, path) != NaviQueryResult::SUCCESS || path.points.size() < 2 ||
                    std::abs(path.points.front().x - 5.f) > .1f || std::abs(path.points.back().x - 15.f) > .1f) {
                    return false;
                }
            }
            return true;
        });
    }
    auto writer = std::async(std::launch::async, [mesh = mesh, &start, &layers] {
        start.wait();
        for (uint32_t repeat = 0; repeat < 32; ++repeat) {
            if (!mesh->RemoveTile({0, 0})) {
                return false;
            }
            for (const auto &layer : layers) {
                if (!mesh->AddTile(layer)) {
                    return false;
                }
            }
        }
        return true;
    });
    start.count_down();
    for (auto &reader : readers) {
        EXPECT_TRUE(reader.get());
    }
    EXPECT_TRUE(writer.get());
}
