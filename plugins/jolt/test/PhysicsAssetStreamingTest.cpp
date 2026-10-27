#include "JoltBackend.h"
#include "JoltWorld.h"
#include "PhysicsAssets.h"

#include <builder/physics/PhysicsAssetBuilder.h>
#include <core/archive/MemoryStreamArchive.h>
#include <framework/asset/AssetManager.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/WorldStreaming.h>
#include <gtest/gtest.h>
#include <jolt/PhysicsSystem.h>
#include <jolt/components/PhysicsBodyComponent.h>
#include <physics/PhysicsBackendRegistry.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <thread>

using namespace sky;
using namespace sky::phy;

namespace {
    class PhysicsAssetStreamingTest : public testing::Test {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(PhysicsBackendRegistry::Get().Register(std::make_unique<JoltBackend>()));
            bundle = Uuid::Create().ToString();
            std::error_code error;
            root = std::filesystem::temp_directory_path(error) / ("sky-physics-assets-" + bundle);
            ASSERT_FALSE(error);
            fs = new NativeFileSystem(FilePath(root));
            AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(fs, bundle));
            host.reset(World::CreateWorld());
            system = AttachPhysicsSystem(*host);
            ASSERT_NE(system, nullptr);
            system->SetSimulating(false);
        }
        void TearDown() override
        {
            host.reset();
            AssetExecutor::Get()->WaitForAll();
            PhysicsBackendRegistry::Get().Unregister();
            fs.Reset(nullptr);
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
        AssetBuildResult Build(const Uuid &id, const std::string &extension, const std::string &text)
        {
            const auto path = FilePath(id.ToString() + extension);
            auto       file = fs->CreateOrOpenFile(path);
            {
                auto output = file->WriteAsArchive();
                EXPECT_TRUE(output->SaveRaw(text.data(), text.size()));
            }
            AssetBuildRequest request;
            request.file            = file;
            request.assetInfo       = new AssetSourceInfo();
            request.assetInfo->uuid = id;
            request.assetInfo->ext  = extension;
            request.assetInfo->path = path;
            request.target = request.bundle = bundle;
            AssetBuildResult    result{};
            PhysicsAssetBuilder builder;
            builder.Request(request, result);
            return result;
        }
        Uuid Collision(float radius = 1)
        {
            const auto      id = Uuid::Create();
            CollisionSource source;
            source.shape.type   = ShapeType::Sphere;
            source.shape.radius = radius;
            source.elements.push_back(Uuid::Create());
            std::string text;
            EXPECT_TRUE(WriteCollision(source, text));
            const auto result = Build(id, ".collision", text);
            EXPECT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
            return id;
        }
        std::unique_ptr<Actor> Body(const Uuid &collision = {}, const Uuid &material = {}, BodyKind kind = BodyKind::Static)
        {
            auto actor = std::make_unique<Actor>(Uuid::Create());
            actor->AddComponent<TransformComponent>();
            auto *body = actor->AddComponent<PhysicsBodyComponent>();
            EXPECT_TRUE(body->SetKind(kind));
            if (collision) {
                EXPECT_TRUE(body->SetCollisionAsset(collision));
                EXPECT_TRUE(body->SetShapeSource(PhysicsShapeSource::Asset));
            }
            EXPECT_TRUE(body->SetMaterialAsset(material));
            return actor;
        }
        Uuid Cell(std::vector<std::unique_ptr<Actor>> actors)
        {
            const auto  id = Uuid::Create();
            std::string error;
            EXPECT_TRUE(host->GetStreaming().AddCell(id, actors, error)) << error;
            return id;
        }
        bool Await(const Uuid &cell, WorldCellState expected)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            do {
                host->Tick(0);
                const auto state = host->GetStreaming().GetState(cell);
                if (state == expected)
                    return true;
                if (state == WorldCellState::Failed)
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (std::chrono::steady_clock::now() < deadline);
            return false;
        }
        std::string Save()
        {
            OMemoryArchive memory;
            {
                JsonOutputArchive output(memory);
                host->SaveJson(output);
            }
            return {memory.Data(), memory.Size()};
        }
        std::filesystem::path  root;
        NativeFileSystemPtr    fs;
        std::string            bundle;
        std::unique_ptr<World> host;
        PhysicsSystem         *system = nullptr;
    };
} // namespace

TEST_F(PhysicsAssetStreamingTest, BuilderToProductsToReadyActivationAndRetirement)
{
    const auto collision = Collision();
    const auto material  = Uuid::Create();
    ASSERT_EQ(Build(material, ".physmat", R"({"version":1,"friction":0.8,"restitution":0.7,"frictionCombine":3})").retCode,
              AssetBuildRetCode::SUCCESS);
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(collision, material));
    const auto actorId = actors.front()->GetUuid();
    const auto cell    = Cell(std::move(actors));
    ASSERT_TRUE(host->GetStreaming().Request(cell, WorldCellTarget::Ready));
    ASSERT_TRUE(Await(cell, WorldCellState::Ready)) << host->GetStreaming().GetError(cell);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_TRUE(host->GetActors().empty());
    PhysicsQueryHit hit;
    EXPECT_FALSE(system->GetWorld()->Raycast({0, 5, 0}, {0, -1, 0}, 10, {}, hit));
    ASSERT_TRUE(host->GetStreaming().Request(cell, WorldCellTarget::Active));
    ASSERT_TRUE(Await(cell, WorldCellState::Active)) << host->GetStreaming().GetError(cell);
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
    ASSERT_TRUE(system->GetWorld()->Raycast({0, 5, 0}, {0, -1, 0}, 10, {}, hit));
    EXPECT_NEAR(hit.distance, 4.f, .01f);
    auto       *body   = host->GetActorByUuid(actorId)->GetComponent<PhysicsBodyComponent>();
    auto       *native = static_cast<JoltWorld *>(system->GetWorld());
    const auto *entry  = native->Resolve(body->GetObjectId());
    ASSERT_NE(entry, nullptr);
    EXPECT_FLOAT_EQ(entry->material.dynamicFriction, .8f);
    EXPECT_FLOAT_EQ(entry->material.restitution, .7f);
    EXPECT_EQ(entry->material.frictionCombine, PhysicsCombineMode::Max);
    EXPECT_TRUE(host->GetStreaming().Request(cell, WorldCellTarget::Unloaded));
    ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_EQ(host->GetActorByUuid(actorId), nullptr);
}

TEST_F(PhysicsAssetStreamingTest, RejectedRecookRetainsPublishedProductAndOldSnapshot)
{
    const auto id    = Collision();
    auto       first = AssetManager::Get()->LoadAsset<CollisionAsset>(id, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(first, nullptr);
    first->BlockUntilLoaded();
    ASSERT_TRUE(first->IsLoaded());
    auto result = Build(id, ".collision", R"({"version":99,"shape":{}})");
    EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
    CollisionSource changed = first->Data();
    changed.shape.radius    = 2;
    std::string text;
    ASSERT_TRUE(WriteCollision(changed, text));
    ASSERT_EQ(Build(id, ".collision", text).retCode, AssetBuildRetCode::SUCCESS);
    auto second = AssetManager::Get()->LoadAsset<CollisionAsset>(id, AssetLoadPolicy::ProductsOnly);
    ASSERT_NE(second, nullptr);
    second->BlockUntilLoaded();
    ASSERT_TRUE(second->IsLoaded());
    EXPECT_NE(first->GetSnapshotGeneration(), second->GetSnapshotGeneration());
    EXPECT_FLOAT_EQ(first->Data().shape.radius, 1.f);
    EXPECT_FLOAT_EQ(second->Data().shape.radius, 2.f);
}

TEST_F(PhysicsAssetStreamingTest, CapacityFailureRollsBackEntireGroup)
{
    PhysicsOptions options;
    options.maxBodies = 1;
    ASSERT_TRUE(system->SetOptions(options));
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body());
    actors.push_back(Body());
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Failed));
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_TRUE(host->GetActors().empty());
    options.maxBodies = 2;
    ASSERT_TRUE(system->SetOptions(options));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active)) << host->GetStreaming().GetError(cell);
    EXPECT_EQ(system->GetStats().bodyCount, 2u);
}

TEST_F(PhysicsAssetStreamingTest, InvalidSecondBodyNeverPublishesFirstBody)
{
    const auto      collision = Collision();
    CollisionSource mesh;
    mesh.elements.push_back(Uuid::Create());
    mesh.shape.type = ShapeType::TriangleMesh;
    const auto id   = Uuid::Create();
    const auto text = std::string(R"({"version":1,"shape":{"id":")") + mesh.elements.front().ToString() +
                      R"(","type":"TriangleMesh","vertices":[[0,0,0],[0,0,1],[1,0,0]],"indices":[0,1,2]}})";
    ASSERT_EQ(Build(id, ".collision", text).retCode, AssetBuildRetCode::SUCCESS);
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(collision));
    actors.push_back(Body(id, {}, BodyKind::Dynamic));
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Failed));
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    EXPECT_TRUE(host->GetActors().empty());
}

TEST_F(PhysicsAssetStreamingTest, CancelledConsumerDoesNotCancelSharedShapeAndWorldMayDie)
{
    auto shape  = std::make_shared<ShapeDesc>();
    auto first  = system->GetWorld()->PrepareShape(shape, VEC3_ONE);
    auto second = system->GetWorld()->PrepareShape(shape, VEC3_ONE);
    ASSERT_EQ(first, second);
    first.reset();
    host.reset();
    PhysicsBackendRegistry::Get().Unregister();
    AssetExecutor::Get()->WaitForAll();
    EXPECT_EQ(second->GetState(), ShapePreparationState::Ready);
    EXPECT_NE(second->GetShape(), nullptr);
}

TEST_F(PhysicsAssetStreamingTest, CancellationAndRetryDoNotInstallStaleBodies)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(Collision()));
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    host->Tick(0);
    host->GetStreaming().Request(cell, WorldCellTarget::Unloaded);
    ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
    AssetExecutor::Get()->WaitForAll();
    host->Tick(0);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active)) << host->GetStreaming().GetError(cell);
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
}

TEST_F(PhysicsAssetStreamingTest, DynamicBodyIsPromotedAndNotDuplicatedWhenCellReloads)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body({}, {}, BodyKind::Dynamic));
    const auto actor = actors.front()->GetUuid();
    const auto cell  = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active));
    const auto body = host->GetActorByUuid(actor)->GetComponent<PhysicsBodyComponent>()->GetObjectId();
    host->GetStreaming().Request(cell, WorldCellTarget::Unloaded);
    ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
    EXPECT_NE(host->GetActorByUuid(actor), nullptr);
    EXPECT_TRUE(system->GetWorld()->HasObject(body));
    EXPECT_FALSE(host->GetStreaming().OwnsActor(actor));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active));
    EXPECT_EQ(system->GetStats().bodyCount, 1u);
}

TEST_F(PhysicsAssetStreamingTest, UnloadedCellsSurviveSceneSaveWithPackageRoots)
{
    const auto                          collision = Collision();
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(collision));
    const auto actor = actors.front()->GetUuid();
    const auto cell  = Cell(std::move(actors));
    const auto saved = Save();
    EXPECT_NE(saved.find(collision.ToString()), std::string::npos);
    EXPECT_NE(saved.find("PhysicsCollision"), std::string::npos);
    IMemoryArchive         memory(saved.data(), saved.size());
    JsonInputArchive       input(memory);
    std::unique_ptr<World> restored(World::CreateWorld());
    ASSERT_TRUE(restored->LoadJson(input)) << restored->GetLastError();
    auto *physics = AttachPhysicsSystem(*restored);
    ASSERT_NE(physics, nullptr);
    EXPECT_TRUE(restored->GetActors().empty());
    EXPECT_TRUE(restored->GetStreaming().OwnsActor(actor));
    EXPECT_EQ(restored->GetStreaming().GetState(cell), WorldCellState::Unloaded);
}

TEST_F(PhysicsAssetStreamingTest, MissingProductsFailWithoutRuntimeCooking)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(Uuid::Create()));
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Failed));
    EXPECT_NE(host->GetStreaming().GetError(cell).find("product"), std::string::npos);
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
}

TEST_F(PhysicsAssetStreamingTest, CrossCellConstraintPinsRetirementUntilExplicitlyRemoved)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body());
    const auto actorId = actors.front()->GetUuid();
    const auto cell    = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active));
    auto           external = Body({}, {}, BodyKind::Dynamic);
    auto          *other    = host->AttachToWorld(std::move(external))->GetComponent<PhysicsBodyComponent>();
    ConstraintDesc desc;
    desc.bodyA            = other->GetObjectId();
    desc.bodyB            = host->GetActorByUuid(actorId)->GetComponent<PhysicsBodyComponent>()->GetObjectId();
    const auto constraint = system->GetWorld()->CreateConstraint(desc);
    ASSERT_TRUE(IsValid(constraint));
    host->GetStreaming().Request(cell, WorldCellTarget::Unloaded);
    host->Tick(0);
    EXPECT_EQ(host->GetStreaming().GetState(cell), WorldCellState::Retiring);
    EXPECT_EQ(system->GetStats().bodyCount, 2u);
    ASSERT_TRUE(system->GetWorld()->DestroyConstraint(constraint));
    ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
    EXPECT_TRUE(system->GetWorld()->HasObject(other->GetObjectId()));
}

TEST_F(PhysicsAssetStreamingTest, SupportingCellStaysResidentUntilDynamicBodyLeaves)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body());
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active));
    auto external = Body({}, {}, BodyKind::Dynamic);
    external->GetComponent<TransformComponent>()->SetLocalTranslation({0, 2, 0});
    auto *other = host->AttachToWorld(std::move(external))->GetComponent<PhysicsBodyComponent>();
    for (uint32_t i = 0; i < 60; ++i)
        ASSERT_TRUE(system->GetWorld()->Step(1.f / 60.f));
    host->GetStreaming().Request(cell, WorldCellTarget::Unloaded);
    host->Tick(0);
    EXPECT_EQ(host->GetStreaming().GetState(cell), WorldCellState::Retiring);
    Transform destination;
    destination.translation = {100, 20, 0};
    ASSERT_TRUE(system->GetWorld()->SetBodyTransform(other->GetObjectId(), destination));
    ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
    EXPECT_TRUE(system->GetWorld()->HasObject(other->GetObjectId()));
}

TEST_F(PhysicsAssetStreamingTest, SceneInitialTargetDrivesStreamingAndReadyDemotesStaticCell)
{
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(Collision()));
    const auto  cell = Uuid::Create();
    std::string error;
    ASSERT_TRUE(host->GetStreaming().AddCell(cell, actors, error, WorldCellTarget::Active));
    const auto saved = Save();
    host.reset(World::CreateWorld());
    IMemoryArchive   stream(saved.data(), saved.size());
    JsonInputArchive input(stream);
    ASSERT_TRUE(host->LoadJson(input));
    system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    system->SetSimulating(false);
    ASSERT_TRUE(Await(cell, WorldCellState::Active)) << host->GetStreaming().GetError(cell);
    host->GetStreaming().Request(cell, WorldCellTarget::Ready);
    ASSERT_TRUE(Await(cell, WorldCellState::Ready));
    EXPECT_EQ(system->GetStats().bodyCount, 0u);
}

TEST_F(PhysicsAssetStreamingTest, LocalRotationIsBakedBeforeActivationAndMaterialCanBeCleared)
{
    CollisionSource source;
    source.elements.push_back(Uuid::Create());
    source.shape.halfExt  = {2, .5f, .5f};
    const float half      = std::sqrt(.5f);
    source.shape.rotation = Quaternion(half, 0, 0, half);
    std::string text;
    ASSERT_TRUE(WriteCollision(source, text));
    const auto collision = Uuid::Create();
    ASSERT_EQ(Build(collision, ".collision", text).retCode, AssetBuildRetCode::SUCCESS);
    const auto material = Uuid::Create();
    ASSERT_EQ(Build(material, ".physmat", R"({"version":1,"friction":0.9})").retCode, AssetBuildRetCode::SUCCESS);
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(collision, material));
    const auto id   = actors.front()->GetUuid();
    const auto cell = Cell(std::move(actors));
    host->GetStreaming().Request(cell, WorldCellTarget::Active);
    ASSERT_TRUE(Await(cell, WorldCellState::Active));
    PhysicsQueryHit hit;
    ASSERT_TRUE(system->GetWorld()->Raycast({0, 5, 0}, {0, -1, 0}, 10, {}, hit));
    EXPECT_NEAR(hit.distance, 3.f, .02f);
    auto *body = host->GetActorByUuid(id)->GetComponent<PhysicsBodyComponent>();
    ASSERT_TRUE(body->SetShapeSource(PhysicsShapeSource::Inline));
    ASSERT_TRUE(body->SetMaterialAsset({}));
    auto *native = static_cast<JoltWorld *>(system->GetWorld());
    EXPECT_FLOAT_EQ(native->Resolve(body->GetObjectId())->material.dynamicFriction, .5f);
}

TEST(PhysicsAssetCodecTest, RejectsDuplicateElementIdsAndNonFiniteAuthoring)
{
    CollisionSource source;
    source.shape.type = ShapeType::Compound;
    source.shape.children.resize(2);
    source.elements = {Uuid::Create(), Uuid::Create(), Uuid::Create()};
    std::string text;
    ASSERT_TRUE(WriteCollision(source, text));
    source.elements.back() = source.elements.front();
    EXPECT_FALSE(WriteCollision(source, text));
    source.elements.back()                  = Uuid::Create();
    source.shape.children.front().halfExt.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(WriteCollision(source, text));
    PhysicsMaterialData material;
    material.linearDamping = 1;
    EXPECT_FALSE(WritePhysicsMaterial(material, text));
}

TEST(PhysicsAssetCodecTest, MalformedCellEnvelopesReturnErrors)
{
    const std::string invalidCells[] = {R"(42)", R"([{"uuid":"00000000-0000-0000-0000-000000000001","initialTarget":256,"actors":[]}])",
                                        R"([{"uuid":"00000000-0000-0000-0000-000000000001","initialTarget":"Active","actors":[]}])",
                                        R"([{"uuid":12,"actors":[]}])",
                                        R"([{"uuid":"00000000-0000-0000-0000-000000000001","actors":[{"uuid":12}]}])"};
    for (const auto &cells : invalidCells) {
        const auto             text = std::string(R"({"actors":[],"subSystems":[],"cells":)") + cells + "}";
        IMemoryArchive         memory(text.data(), text.size());
        JsonInputArchive       input(memory);
        std::unique_ptr<World> world(World::CreateWorld());
        EXPECT_FALSE(world->LoadJson(input));
        EXPECT_FALSE(world->GetLastError().empty());
        EXPECT_TRUE(world->GetActors().empty());
    }
}

TEST_F(PhysicsAssetStreamingTest, RepeatedPagingSharesAndReleasesPreparedResources)
{
    const auto                          collision = Collision();
    std::vector<std::unique_ptr<Actor>> actors;
    actors.push_back(Body(collision));
    actors.push_back(Body(collision));
    const auto cell  = Cell(std::move(actors));
    const auto start = std::chrono::steady_clock::now();
    for (uint32_t cycle = 0; cycle < 20; ++cycle) {
        host->GetStreaming().Request(cell, WorldCellTarget::Active);
        ASSERT_TRUE(Await(cell, WorldCellState::Active)) << host->GetStreaming().GetError(cell);
        EXPECT_EQ(system->GetStats().shapeResourceCount, 1u);
        EXPECT_EQ(system->GetStats().bodyCount, 2u);
        host->GetStreaming().Request(cell, WorldCellTarget::Unloaded);
        ASSERT_TRUE(Await(cell, WorldCellState::Unloaded));
        AssetExecutor::Get()->WaitForAll();
        EXPECT_EQ(system->GetStats().shapeResourceCount, 0u);
        EXPECT_EQ(system->GetStats().shapePreparationCount, 0u);
        EXPECT_EQ(system->GetStats().shapeSourceBytes, 0u);
    }
    RecordProperty("paging_cycles", 20);
    RecordProperty("paging_milliseconds",
                   static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));
}

TEST_F(PhysicsAssetStreamingTest, SavedProjectFixtureLoadsBothCellsFromIndependentProducts)
{
    NativeFileSystem                            sources(FilePath(std::filesystem::path(__FILE__).parent_path() / "assets"));
    const std::pair<const char *, const char *> products[] = {{"streaming-box.collision", "28C4B9A5-F1E2-57DA-BC9C-3F66825E7A1A"},
                                                              {"streaming-plane.collision", "7DAA5807-F557-5DDC-9AC2-4DF5EECEFFDC"},
                                                              {"streaming-rubber.physmat", "879CAAE4-BB15-56E5-B2E7-254DAB2FBA2D"}};
    for (const auto &[name, id] : products) {
        auto file = sources.OpenFile(name);
        ASSERT_NE(file, nullptr);
        std::string source;
        ASSERT_TRUE(file->ReadString(source));
        const auto extension = std::filesystem::path(name).extension().string();
        const auto result    = Build(Uuid::CreateFromString(id), extension, source);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
    }
    auto file = sources.OpenFile("streaming.world");
    ASSERT_NE(file, nullptr);
    auto stream = file->ReadAsArchive();
    ASSERT_NE(stream, nullptr);
    JsonInputArchive input(*stream);
    host.reset(World::CreateWorld());
    ASSERT_TRUE(host->LoadJson(input)) << host->GetLastError();
    system = AttachPhysicsSystem(*host);
    ASSERT_NE(system, nullptr);
    system->SetSimulating(false);
    std::string text;
    ASSERT_TRUE(file->ReadString(text));
    rapidjson::Document document;
    document.Parse(text.c_str());
    ASSERT_FALSE(document.HasParseError());
    const auto active    = Uuid::CreateFromString(document["cells"][0]["uuid"].GetString());
    const auto preloaded = Uuid::CreateFromString(document["cells"][1]["uuid"].GetString());
    ASSERT_TRUE(Await(active, WorldCellState::Active)) << host->GetStreaming().GetError(active);
    ASSERT_TRUE(Await(preloaded, WorldCellState::Ready)) << host->GetStreaming().GetError(preloaded);
    EXPECT_EQ(system->GetStats().bodyCount, 6u);
    PhysicsQueryHit hit;
    EXPECT_FALSE(system->GetWorld()->Raycast({30, 5, 0}, {0, -1, 0}, 10, {}, hit));
    ASSERT_TRUE(host->GetStreaming().Request(preloaded, WorldCellTarget::Active));
    ASSERT_TRUE(Await(preloaded, WorldCellState::Active));
    EXPECT_EQ(system->GetStats().bodyCount, 7u);
    ASSERT_TRUE(system->GetWorld()->Raycast({30, 5, 0}, {0, -1, 0}, 10, {}, hit));
    EXPECT_NEAR(hit.distance, 5.f, .01f);
}

namespace {
    using BenchmarkClock = std::chrono::steady_clock;

    double MillisecondsSince(BenchmarkClock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(BenchmarkClock::now() - start).count();
    }

    void ReportSamples(const std::string &name, std::vector<double> samples)
    {
        if (samples.empty())
            return;
        double total = 0;
        for (const double sample : samples)
            total += sample;
        std::sort(samples.begin(), samples.end());
        const auto percentile = [&](double fraction) { return samples[static_cast<size_t>(std::ceil(fraction * samples.size())) - 1]; };
        testing::Test::RecordProperty(name + "_samples", std::to_string(samples.size()));
        testing::Test::RecordProperty(name + "_total_ms", std::to_string(total));
        testing::Test::RecordProperty(name + "_p50_ms", std::to_string(percentile(.5)));
        testing::Test::RecordProperty(name + "_p95_ms", std::to_string(percentile(.95)));
        testing::Test::RecordProperty(name + "_max_ms", std::to_string(samples.back()));
        std::printf("[Benchmark] %s n=%zu total=%.3f ms p50=%.3f ms p95=%.3f ms max=%.3f ms\n", name.c_str(), samples.size(), total, percentile(.5),
                    percentile(.95), samples.back());
    }

    struct StreamingPhaseSamples {
        std::vector<double> wall;
        std::vector<double> ticks;
        double              pollWaitMilliseconds    = 0;
        double              workerDrainMilliseconds = 0;

        void Report(const std::string &name) const
        {
            ReportSamples(name + "_wall", wall);
            ReportSamples(name + "_host_tick", ticks);
            testing::Test::RecordProperty(name + "_poll_wait_ms", std::to_string(pollWaitMilliseconds));
            testing::Test::RecordProperty(name + "_worker_drain_ms", std::to_string(workerDrainMilliseconds));
        }
    };

    class PhysicsStreamingBenchmark : public PhysicsAssetStreamingTest {
    protected:
        void SetUp() override
        {
            PhysicsAssetStreamingTest::SetUp();
            RecordProperty("poll_sleep_ms", 1);
            RecordProperty("simulation_enabled", 0);
            RecordProperty("os_file_cache", "Uncontrolled; freshly cooked inputs may be OS-cached");
            RecordProperty("memory_scope", "Sampled prepared-shape source bytes; excludes native allocator and process memory");
#if defined(_DEBUG)
            RecordProperty("build_mode", "Debug");
#else
            RecordProperty("build_mode", "NonDebug");
#endif
        }

        void TearDown() override
        {
            cook.Report("fixture_publish");
            RecordProperty("sampled_peak_bodies", std::to_string(peak.bodyCount));
            RecordProperty("sampled_peak_shape_resources", std::to_string(peak.shapeResourceCount));
            RecordProperty("sampled_peak_preparation_tasks", std::to_string(peak.shapePreparationCount));
            RecordProperty("sampled_peak_shape_source_bytes", std::to_string(peak.shapeSourceBytes));
            std::printf("[Benchmark] peaks: bodies=%u shapes=%u preparing=%u source_bytes=%llu\n", peak.bodyCount, peak.shapeResourceCount,
                        peak.shapePreparationCount, static_cast<unsigned long long>(peak.shapeSourceBytes));
            PhysicsAssetStreamingTest::TearDown();
        }

        bool Publish(const ShapeDesc &shape, Uuid &id)
        {
            CollisionSource source;
            source.shape = shape;
            source.elements.push_back(Uuid::Create());
            std::string text;
            const auto  status = WriteCollision(source, text);
            if (!status) {
                ADD_FAILURE() << status.message;
                return false;
            }
            id = Uuid::Create();
            return PublishText(id, ".collision", text);
        }

        bool PublishText(const Uuid &id, const std::string &extension, const std::string &text)
        {
            const auto start  = BenchmarkClock::now();
            const auto result = Build(id, extension, text);
            cook.wall.push_back(MillisecondsSince(start));
            if (result.retCode != AssetBuildRetCode::SUCCESS) {
                ADD_FAILURE() << result.error;
                return false;
            }
            return true;
        }

        bool AddCells(uint32_t count, uint32_t bodiesPerCell, const std::vector<Uuid> &shapes, const Uuid &material, std::vector<Uuid> &cells)
        {
            if (shapes.empty())
                return false;
            for (uint32_t cell = 0; cell < count; ++cell) {
                std::vector<std::unique_ptr<Actor>> actors;
                for (uint32_t body = 0; body < bodiesPerCell; ++body) {
                    const auto index = cell * bodiesPerCell + body;
                    auto       actor = Body(shapes[index % shapes.size()], material);
                    actor->GetComponent<TransformComponent>()->SetLocalTranslation(
                        {static_cast<float>(index % 64) * 4.f, 0, static_cast<float>(index / 64) * 4.f});
                    actors.push_back(std::move(actor));
                }
                const auto  id = Uuid::Create();
                std::string error;
                if (!host->GetStreaming().AddCell(id, actors, error)) {
                    ADD_FAILURE() << error;
                    return false;
                }
                cells.push_back(id);
            }
            return true;
        }

        void SampleResidency()
        {
            const auto stats           = system->GetStats();
            peak.bodyCount             = std::max(peak.bodyCount, stats.bodyCount);
            peak.shapeResourceCount    = std::max(peak.shapeResourceCount, stats.shapeResourceCount);
            peak.shapePreparationCount = std::max(peak.shapePreparationCount, stats.shapePreparationCount);
            peak.shapeSourceBytes      = std::max(peak.shapeSourceBytes, stats.shapeSourceBytes);
        }

        template <typename Predicate>
        bool TickUntil(const std::vector<Uuid> &cells, Predicate reached, StreamingPhaseSamples &samples, BenchmarkClock::time_point start)
        {
            const auto deadline = start + std::chrono::seconds(30);
            do {
                const auto tickStart = BenchmarkClock::now();
                host->Tick(0);
                samples.ticks.push_back(MillisecondsSince(tickStart));
                SampleResidency();
                for (const auto &cell : cells) {
                    if (host->GetStreaming().GetState(cell) == WorldCellState::Failed) {
                        ADD_FAILURE() << host->GetStreaming().GetError(cell);
                        return false;
                    }
                }
                if (reached())
                    return true;
                // Wall latency includes this polling cadence; host_tick samples exclude it.
                const auto waitStart = BenchmarkClock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                samples.pollWaitMilliseconds += MillisecondsSince(waitStart);
            } while (BenchmarkClock::now() < deadline);
            ADD_FAILURE() << "Streaming benchmark did not reach its target state within 30 seconds";
            return false;
        }

        bool Transition(const std::vector<Uuid> &cells, WorldCellTarget target, WorldCellState expected, StreamingPhaseSamples &samples)
        {
            const auto start = BenchmarkClock::now();
            for (const auto &cell : cells) {
                if (!host->GetStreaming().Request(cell, target))
                    return false;
            }
            if (!TickUntil(
                    cells,
                    [&] {
                        return std::all_of(cells.begin(), cells.end(),
                                           [&](const auto &cell) { return host->GetStreaming().GetState(cell) == expected; });
                    },
                    samples, start))
                return false;
            if (expected == WorldCellState::Unloaded) {
                const auto drainStart = BenchmarkClock::now();
                AssetExecutor::Get()->WaitForAll();
                samples.workerDrainMilliseconds += MillisecondsSince(drainStart);
            }
            samples.wall.push_back(MillisecondsSince(start));
            SampleResidency();
            return true;
        }

        void CheckReleased(uint32_t residentShapes = 0)
        {
            const auto stats = system->GetStats();
            EXPECT_EQ(stats.bodyCount, 0u);
            EXPECT_EQ(stats.shapePreparationCount, 0u);
            EXPECT_EQ(stats.shapeResourceCount, residentShapes);
            if (!residentShapes)
                EXPECT_EQ(stats.shapeSourceBytes, 0u);
            EXPECT_TRUE(host->GetActors().empty());
        }

        void RunPaging(const std::vector<Uuid> &cells, uint32_t bodiesPerCell, uint32_t shapes, uint32_t cycles, uint32_t retainedShapes = 0)
        {
            RecordProperty("cells", std::to_string(cells.size()));
            RecordProperty("bodies_per_cell", std::to_string(bodiesPerCell));
            RecordProperty("unique_shapes", std::to_string(shapes));
            RecordProperty("paging_cycles", std::to_string(cycles));
            StreamingPhaseSamples prepare, activate, retire;
            for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
                SCOPED_TRACE(cycle);
                ASSERT_TRUE(Transition(cells, WorldCellTarget::Ready, WorldCellState::Ready, prepare));
                EXPECT_EQ(system->GetStats().bodyCount, 0u);
                EXPECT_EQ(system->GetStats().shapeResourceCount, shapes);
                EXPECT_TRUE(host->GetActors().empty());
                PhysicsQueryHit hit;
                EXPECT_FALSE(system->GetWorld()->Raycast({.25f, 5, .25f}, {0, -1, 0}, 10, {}, hit));
                ASSERT_TRUE(Transition(cells, WorldCellTarget::Active, WorldCellState::Active, activate));
                EXPECT_EQ(system->GetStats().bodyCount, cells.size() * bodiesPerCell);
                EXPECT_EQ(host->GetActors().size(), cells.size() * bodiesPerCell);
                EXPECT_TRUE(system->GetWorld()->Raycast({.25f, 5, .25f}, {0, -1, 0}, 10, {}, hit));
                ASSERT_TRUE(Transition(cells, WorldCellTarget::Unloaded, WorldCellState::Unloaded, retire));
                CheckReleased(retainedShapes);
                EXPECT_FALSE(system->GetWorld()->Raycast({.25f, 5, .25f}, {0, -1, 0}, 10, {}, hit));
            }
            RecordProperty("first_prepare_wall_ms", std::to_string(prepare.wall.front()));
            prepare.Report("prepare");
            activate.Report("activate");
            retire.Report("retire_and_drain");
        }

        bool PublishGrid(uint32_t side, Uuid &id)
        {
            ShapeDesc shape;
            shape.type     = ShapeType::TriangleMesh;
            shape.meshData = new TriangleMesh();
            std::vector<Vector3>  vertices;
            std::vector<uint32_t> indices;
            for (uint32_t z = 0; z <= side; ++z)
                for (uint32_t x = 0; x <= side; ++x)
                    vertices.emplace_back(static_cast<float>(x), 0.f, static_cast<float>(z));
            for (uint32_t z = 0; z < side; ++z) {
                for (uint32_t x = 0; x < side; ++x) {
                    const auto a = z * (side + 1) + x;
                    const auto b = a + side + 1;
                    indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
                }
            }
            shape.meshData->position.resize(vertices.size() * sizeof(Vector3));
            shape.meshData->indexRaw.resize(indices.size() * sizeof(uint32_t));
            std::memcpy(shape.meshData->position.data(), vertices.data(), shape.meshData->position.size());
            std::memcpy(shape.meshData->indexRaw.data(), indices.data(), shape.meshData->indexRaw.size());
            RecordProperty("mesh_vertices", std::to_string(vertices.size()));
            RecordProperty("mesh_triangles", std::to_string(indices.size() / 3));
            return Publish(shape, id);
        }

        StreamingPhaseSamples cook;
        PhysicsWorldStats     peak;
    };
} // namespace

TEST_F(PhysicsStreamingBenchmark, DISABLED_SharedCollisionAndMaterialPaging)
{
    Uuid shape;
    ASSERT_TRUE(Publish(ShapeDesc{}, shape));
    const auto material = Uuid::Create();
    ASSERT_TRUE(PublishText(material, ".physmat", R"({"version":1,"friction":0.8,"restitution":0.1})"));
    std::vector<Uuid> cells;
    ASSERT_TRUE(AddCells(4, 64, {shape}, material, cells));
    RunPaging(cells, 64, 1, 20);
}

TEST_F(PhysicsStreamingBenchmark, DISABLED_UniqueCollisionPaging)
{
    std::vector<Uuid> shapes;
    for (uint32_t index = 0; index < 128; ++index) {
        ShapeDesc shape;
        shape.type   = ShapeType::Sphere;
        shape.radius = 1.f + static_cast<float>(index) * .001f;
        Uuid id;
        ASSERT_TRUE(Publish(shape, id));
        shapes.push_back(id);
    }
    std::vector<Uuid> cells;
    ASSERT_TRUE(AddCells(4, 32, shapes, {}, cells));
    RunPaging(cells, 32, 128, 5);
}

TEST_F(PhysicsStreamingBenchmark, DISABLED_ResidentShapeReuseAcrossCells)
{
    Uuid shape;
    ASSERT_TRUE(Publish(ShapeDesc{}, shape));
    const auto material = Uuid::Create();
    ASSERT_TRUE(PublishText(material, ".physmat", R"({"version":1,"friction":0.8,"restitution":0.1})"));
    std::vector<Uuid> anchor, cells;
    ASSERT_TRUE(AddCells(1, 1, {shape}, material, anchor));
    ASSERT_TRUE(AddCells(4, 64, {shape}, material, cells));
    StreamingPhaseSamples anchorPrepare, anchorRetire;
    ASSERT_TRUE(Transition(anchor, WorldCellTarget::Ready, WorldCellState::Ready, anchorPrepare));
    ASSERT_NO_FATAL_FAILURE(RunPaging(cells, 64, 1, 20, 1));
    ASSERT_TRUE(Transition(anchor, WorldCellTarget::Unloaded, WorldCellState::Unloaded, anchorRetire));
    CheckReleased();
    anchorPrepare.Report("anchor_prepare");
    anchorRetire.Report("anchor_retire");
}

TEST_F(PhysicsStreamingBenchmark, DISABLED_TriangleMeshPaging)
{
    Uuid shape;
    ASSERT_TRUE(PublishGrid(128, shape));
    std::vector<Uuid> cells;
    ASSERT_TRUE(AddCells(4, 16, {shape}, {}, cells));
    RunPaging(cells, 16, 1, 5);
}

TEST_F(PhysicsStreamingBenchmark, DISABLED_CancelAdmittedPreparationAndReload)
{
    Uuid shape;
    ASSERT_TRUE(PublishGrid(64, shape));
    std::vector<Uuid> cells;
    ASSERT_TRUE(AddCells(16, 16, {shape}, {}, cells));
    StreamingPhaseSamples admission, cancel;
    uint32_t              cancelledWhileNativeTaskPending = 0;
    for (uint32_t cycle = 0; cycle < 10; ++cycle) {
        const auto start = BenchmarkClock::now();
        for (const auto &cell : cells)
            ASSERT_TRUE(host->GetStreaming().Request(cell, WorldCellTarget::Ready));
        ASSERT_TRUE(TickUntil(cells, [&] { return system->GetStats().shapeResourceCount != 0; }, admission, start));
        admission.wall.push_back(MillisecondsSince(start));
        if (system->GetStats().shapePreparationCount)
            ++cancelledWhileNativeTaskPending;
        ASSERT_EQ(system->GetStats().bodyCount, 0u);
        ASSERT_TRUE(Transition(cells, WorldCellTarget::Unloaded, WorldCellState::Unloaded, cancel));
        CheckReleased();
    }
    RecordProperty("cancel_cycles", 10);
    RecordProperty("cancel_cycles_with_pending_native_task", std::to_string(cancelledWhileNativeTaskPending));
    admission.Report("cancel_admission");
    cancel.Report("cancel_and_drain");
    RunPaging(cells, 16, 1, 2);
}
