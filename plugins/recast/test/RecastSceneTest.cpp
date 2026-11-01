//
// Created on 2026/10/30.
//

#include <framework/application/ModuleManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetIndexFile.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProductBundle.h>
#include <framework/compression/Compressor.h>
#include <framework/world/WorldDesc.h>
#include <navigation/INavigationSystem.h>
#include <navigation/NaviMeshAsset.h>
#include <navigation/NaviMeshFactory.h>
#include <recast/NavigationSubSystemConfig.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>

using namespace sky;
using namespace sky::ai;

namespace {
    class RecastSceneTest : public testing::Test {
    protected:
        void SetUp() override
        {
            const char *configured = std::getenv("SKY_NAVIGATION_SCENE_PROJECT");
            if (!configured || !*configured)
                GTEST_SKIP() << "Set SKY_NAVIGATION_SCENE_PROJECT to the cooked validation project";
            if (!ModuleManager::GetBuildOption("SKY_BUILD_JOLT").value_or(false) ||
                !CompressionManager::Get()->AcquireCompressor(CompressionMethod::LZ4))
                GTEST_SKIP() << "The scene validation requires Jolt and Framework LZ4 support";

            project = std::filesystem::path(configured);
            std::error_code error;
            ASSERT_TRUE(std::filesystem::is_directory(project / "products" / "common", error));
            ASSERT_FALSE(error);
            modules.RegisterModule({"JoltPhysicsModule", {}});
            modules.RegisterModule({"RecastNavigation", {}});
            modules.TopoSort();
            ASSERT_TRUE(modules.LoadModules({}));
            modules.StartModules();

            NativeFileSystemPtr source    = new NativeFileSystem(FilePath(project / "assets"));
            const auto          manifests = NativeFileSystem::FilterFiles(source->GetPath(), ".jsonl");
            ASSERT_TRUE(std::holds_alternative<std::vector<FilePath>>(manifests)) << "Could not scan source manifests";
            for (const auto &file : std::get<std::vector<FilePath>>(manifests)) {
                if (file.FileName() != "assets.jsonl")
                    continue;
                const auto manifest = AssetIndexFile::Load(source, file, "file", "cook");
                for (const auto &entry : manifest.Entries()) {
                    if (entry.key.ends_with(".navmesh")) {
                        ASSERT_FALSE(meshId) << "The validation project must identify exactly one .navmesh source";
                        meshId = entry.id;
                    }
                }
            }
            ASSERT_TRUE(meshId) << "No .navmesh identity was found in the source manifests";
            auto *database = AssetDataBase::Get();
            database->SetWorkSpaceFs(source);
            AssetManager::Get()->SetSourceCatalog(database);
            AssetManager::Get()->SetWorkFileSystem(new NativeFileSystem(FilePath(project)));
            NativeFileSystemPtr products = new NativeFileSystem(FilePath(project / "products" / "common"));
            AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(products, "common"));
            AssetManager::Get()->RefreshProductIndex();
        }

        void TearDown() override
        {
            TaskExecutor::Get()->WaitForAll();
            world = nullptr;
            AssetExecutor::Get()->WaitForAll();
            AssetManager::Get()->SetSourceCatalog(nullptr);
            AssetDataBase::Get()->SetWorkSpaceFs(nullptr);
            modules.UnLoadModules();
        }

        void SaveArtifacts(const NaviMeshData &data, const NaviDebugGeometry &geometry, const NaviPath &path)
        {
            std::error_code error;
            const auto      output = project / "validation";
            std::filesystem::create_directories(output, error);
            ASSERT_FALSE(error) << error.message();
            std::ofstream obj(output / "navmesh.obj");
            ASSERT_TRUE(obj.is_open());
            obj << "# Cooked validation navigation mesh\n" << std::setprecision(9);
            for (const auto &vertex : geometry.vertices)
                obj << "v " << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z << '\n';
            for (size_t i = 0; i < geometry.vertices.size(); i += 3)
                obj << "f " << i + 1 << ' ' << i + 2 << ' ' << i + 3 << '\n';
            obj.close();
            ASSERT_TRUE(obj.good());

            std::ofstream json(output / "paths.json");
            ASSERT_TRUE(json.is_open());
            json << std::setprecision(9) << "{\n  \"asset\": \"" << meshId.ToString() << "\",\n  \"tileCount\": " << data.tiles.size()
                 << ",\n  \"triangleCount\": " << geometry.vertices.size() / 3
                 << ",\n  \"reachable\": true,\n  \"islandReachable\": false,\n  \"points\": [";
            float length = 0;
            for (size_t i = 0; i < path.points.size(); ++i) {
                const auto &point = path.points[i];
                json << (i ? "," : "") << "\n    [" << point.x << ", " << point.y << ", " << point.z << ']';
                if (i) {
                    const auto delta = point - path.points[i - 1];
                    length += std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                }
            }
            json << "\n  ],\n  \"length\": " << length << "\n}\n";
            json.close();
            ASSERT_TRUE(json.good());
        }

        ModuleManager         modules;
        std::filesystem::path project;
        Uuid                  meshId;
        WorldPtr              world;
    };
} // namespace

TEST_F(RecastSceneTest, CookedSceneRoutesAroundWallAndRejectsDisconnectedIsland)
{
    AssetProductInspection inspection;
    const auto             inspected = AssetManager::Get()->InspectProduct(meshId, "common", inspection);
    ASSERT_TRUE(inspected) << inspected.message;
    ASSERT_EQ(inspection.type, "NaviMesh");
    const auto asset = AssetManager::Get()->LoadAsset<NaviMesh>(meshId, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(asset, nullptr);
    asset->BlockUntilLoaded();
    ASSERT_TRUE(asset->IsLoaded());
    ASSERT_GT(asset->Data().tiles.size(), 1u);

    NavigationSubSystemConfig config;
    config.navMeshAsset = meshId;
    config.streaming    = false;
    WorldDesc description;
    description.subSystems.push_back({Name(INavigationSystem::NAME.data()), Any(std::in_place_type<NavigationSubSystemConfig>, config), true});
    world = World::CreateWorld();
    ASSERT_TRUE(world->Build(description)) << world->GetLastError();
    world->Init();
    auto *navigation = static_cast<INavigationSystem *>(world->GetSubSystem(Name(INavigationSystem::NAME.data())));
    ASSERT_NE(navigation, nullptr);
    const auto &mesh = navigation->GetNaviMesh();
    ASSERT_NE(mesh, nullptr);
    const auto filter = NaviMeshFactory::Get()->CreateQueryFilter();
    ASSERT_NE(filter, nullptr);

    NaviPath around;
    ASSERT_EQ(mesh->QueryPath({-8, 0, 0}, {8, 0, 0}, filter, {}, around), NaviQueryResult::SUCCESS);
    ASSERT_GT(around.points.size(), 2u);
    EXPECT_NEAR(around.points.front().x, -8, .2f);
    EXPECT_NEAR(around.points.back().x, 8, .2f);
    EXPECT_TRUE(std::ranges::any_of(around.points, [](const auto &point) { return std::abs(point.z) > 3.f; }));
    for (const auto &point : around.points)
        EXPECT_LT(point.y, 1.f) << "The route must go around the wall rather than walk over it";
    NaviPath island;
    EXPECT_EQ(mesh->QueryPath({-8, 0, 0}, {20, 0, 0}, filter, {}, island), NaviQueryResult::FAILED);
    EXPECT_TRUE(island.points.empty());
    const auto asynchronous = navigation->RequestPath({-8, 0, 0}, {8, 0, 0}, filter);
    ASSERT_NE(asynchronous, nullptr);
    TaskExecutor::Get()->WaitForAll();
    ASSERT_TRUE(asynchronous->IsFinished());
    EXPECT_EQ(asynchronous->GetResult(), NaviQueryResult::SUCCESS);

    NaviDebugGeometry geometry;
    mesh->BuildDebugGeometry(geometry);
    ASSERT_FALSE(geometry.vertices.empty());
    ASSERT_EQ(geometry.vertices.size() % 3, 0u);
    SaveArtifacts(asset->Data(), geometry, around);
}
