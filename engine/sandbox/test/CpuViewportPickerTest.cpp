//
// Created on 2026/10/23.
//

#include <editor/render/CpuViewportPicker.h>

#include "ViewportPickIndex.h"
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::editor;
namespace {
    std::shared_ptr<SceneMeshResources>
    PickMesh(std::vector<Vector3> vertices = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}, uint64_t revision = 1, uint32_t seed = 1)
    {
        auto                  result = std::make_shared<SceneMeshResources>();
        RenderResourcePayload payload;
        payload.resource = {Uuid::CreateWithSeed(seed), revision, 1, 1};
        payload.format   = Name("runtime-mesh");
        auto &mesh       = payload.mesh.emplace();
        mesh.streams.push_back({0, vertices.size() * sizeof(Vector3), sizeof(Vector3)});
        mesh.attributes.push_back({.format = Format::F_RGB32, .semantic = "POSITION"});
        const size_t indexOffset = vertices.size() * sizeof(Vector3);
        payload.bytes.resize(indexOffset + vertices.size() * sizeof(uint32_t));
        std::memcpy(payload.bytes.data(), vertices.data(), indexOffset);
        for (uint32_t i = 0; i < vertices.size(); ++i)
            std::memcpy(payload.bytes.data() + indexOffset + i * sizeof(uint32_t), &i, sizeof(i));
        RenderMeshLod lod;
        lod.offset             = indexOffset;
        lod.size               = vertices.size() * sizeof(uint32_t);
        lod.command.indexCount = static_cast<uint32_t>(vertices.size());
        mesh.lods.push_back(lod);
        AABB bounds{Vector3{INFINITY}, Vector3{-INFINITY}};
        for (auto point : vertices) {
            bounds.min = Min(bounds.min, point);
            bounds.max = Max(bounds.max, point);
        }
        payload.metadata.bounds = bounds;
        result->parts.push_back({.mesh = {payload.resource, 1}, .surface = SceneSurfaceFacts::StaticOpaque()});
        result->payloads.push_back(std::move(payload));
        return result;
    }
    SceneGeometryInstance Instance(std::shared_ptr<const SceneMeshResources> mesh, float z = 0, uint32_t id = 100)
    {
        SceneGeometryInstance result;
        result.actor       = Uuid::CreateWithSeed(id);
        result.world       = RenderIdentityMatrix();
        result.world.v[14] = z;
        result.resources   = std::move(mesh);
        result.state       = SceneGeometryState::READY;
        return result;
    }
    class CpuViewportPickerTest : public testing::Test {
    protected:
        std::shared_ptr<SceneGeometrySnapshot> scene = std::make_shared<SceneGeometrySnapshot>();
        ViewportPickRequest                    request{{.document = 1, .scene = 1, .camera = 1, .request = 1}, {0, 0, 5}, {0, 0, -1}, .1f, 100.f};
        CpuViewportPicker                      picker;
        ViewportPickResult                     Resolve(CpuViewportPicker &target, std::chrono::seconds timeout = std::chrono::seconds(10))
        {
            scene->revision          = request.context.scene;
            const auto         until = std::chrono::steady_clock::now() + timeout;
            ViewportPickResult result;
            do {
                target.Prepare(scene, request.context);
                request.context.providers = target.Context().providers;
                result                    = target.Query(request);
                if (result.status != ViewportPickStatus::PENDING)
                    return result;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (std::chrono::steady_clock::now() < until);
            ADD_FAILURE() << "Preparation timed out: " << result.message;
            return result;
        }
        ViewportPickResult Resolve()
        {
            return Resolve(picker);
        }
        ViewportPickResult MeasureWarmQuery(double &milliseconds)
        {
            const auto         start = std::chrono::steady_clock::now();
            ViewportPickResult result;
            do {
                result       = picker.Query(request);
                milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            } while (result.status == ViewportPickStatus::PENDING && milliseconds < 10000);
            return result;
        }
    };
} // namespace

TEST_F(CpuViewportPickerTest, NearestTrianglesIgnoreBoundsHolesAndNeedNoCollider)
{
    scene->instances = {Instance(PickMesh(), 0, 1), Instance(PickMesh({{-.9f, -1, 0}, {1, -1, 0}, {1, .9f, 0}}, 1, 2), 2, 2)};
    const auto hit   = Resolve();
    ASSERT_EQ(hit.status, ViewportPickStatus::HIT) << hit.message;
    EXPECT_EQ(hit.actor, Uuid::CreateWithSeed(1));
    EXPECT_FLOAT_EQ(hit.distance, 5);
    request.origin.x = .8f;
    request.origin.y = -.8f;
    EXPECT_EQ(Resolve().actor, Uuid::CreateWithSeed(2));
    request.origin = {4, 0, 5};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
}

TEST_F(CpuViewportPickerTest, WorldDistanceClippingNegativeAndNonUniformScale)
{
    auto a = Instance(PickMesh(), 1, 1), b = Instance(PickMesh(), -2, 2);
    a.world.v[0]     = -2;
    a.world.v[5]     = .5f;
    a.world.v[10]    = 10;
    b.world.v[10]    = .1f;
    scene->instances = {b, a};
    EXPECT_EQ(Resolve().actor, a.actor);
    request.nearDistance = 5;
    EXPECT_EQ(Resolve().actor, b.actor);
    request.farDistance = 6;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
    request.origin       = {0, 0, -5};
    request.direction    = {0, 0, 1};
    request.nearDistance = .1f;
    request.farDistance  = 100;
    EXPECT_EQ(Resolve().actor, b.actor);
}

TEST_F(CpuViewportPickerTest, ParallelBoundaryRayHitsAndDegeneratesAreSkipped)
{
    scene->instances = {Instance(PickMesh({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}}))};
    auto result      = Resolve();
    EXPECT_EQ(result.status, ViewportPickStatus::HIT) << result.message;
    EXPECT_EQ(picker.Statistics().degenerateTriangles, 1u);
    EXPECT_EQ(picker.Statistics().triangles, 1u);
    scene->instances[0].world.v[0] = 0;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
}

TEST_F(CpuViewportPickerTest, InvalidRangesAndPositionsReturnExplicitErrors)
{
    auto mesh                                 = PickMesh();
    mesh->payloads[0].mesh->streams[0].stride = 4;
    scene->instances                          = {Instance(mesh)};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
    scene->instances = {Instance(PickMesh({{NAN, 0, 0}, {1, 0, 0}, {0, 1, 0}}, 2))};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
}

TEST_F(CpuViewportPickerTest, IncompleteCandidatesBlockOnlyPotentialNearerHits)
{
    scene->instances = {Instance(PickMesh())};
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    auto unresolved   = Instance({}, 2, 2);
    unresolved.state  = SceneGeometryState::PENDING;
    unresolved.bounds = AABB{{-1, -1, -.1f}, {1, 1, .1f}};
    scene->instances.push_back(unresolved);
    picker.Prepare(scene, request.context);
    EXPECT_EQ(picker.Query(request).status, ViewportPickStatus::PENDING);
    scene->instances[1].world.v[12] = 10;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    scene->instances[1].world.v[12] = 0;
    scene->instances[1].world.v[14] = -3;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    scene->instances[1].world.v[14] = 2;
    scene->instances[1].state       = SceneGeometryState::UNSUPPORTED;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
    scene->instances[1].state = SceneGeometryState::ERROR;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
    scene->instances[1].visibilityMask = 0;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
}

TEST_F(CpuViewportPickerTest, MaterialSupportAndU16SectionsAreExplicit)
{
    Asset<Mesh> asset;
    asset.SetUuid(Uuid::CreateWithSeed(50));
    asset.Data()           = ScenePrimitiveMesh(ScenePrimitive::CUBE);
    asset.Data().subMeshes = {{0, 6, 0}, {6, 30, 1}};
    auto converted         = ConvertSceneMesh(asset, {API::VULKAN, 1});
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(converted));
    auto data = std::make_shared<SceneMeshResources>();
    data->payloads.push_back(std::get<RenderResourcePayload>(converted));
    data->parts.push_back({.mesh = {data->payloads[0].resource, 1}, .surface = SceneSurfaceFacts::StaticOpaque()});
    data->parts.push_back({.part = 1, .mesh = {data->payloads[0].resource, 1}, .surface = SceneSurfaceFacts::StaticOpaque()});
    scene->instances = {Instance(data)};
    EXPECT_NEAR(Resolve().distance, 4.5f, 1.e-5f);
    EXPECT_EQ(Resolve().part, 1u);
    data->parts[0].surface = {};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
}

TEST_F(CpuViewportPickerTest, SharedGeometryReusesBvhAcrossInstancesAndTransforms)
{
    auto data = PickMesh();
    for (uint32_t i = 0; i < 100; ++i)
        scene->instances.push_back(Instance(data, -float(i), i));
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    EXPECT_EQ(picker.Statistics().builds, 1u);
    scene->instances[0].world.v[0] = -2;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    EXPECT_EQ(picker.Statistics().builds, 1u);
    scene->instances = {Instance(PickMesh({{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}, 2))};
    ++request.context.scene;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    EXPECT_EQ(picker.Statistics().builds, 2u);
}

TEST_F(CpuViewportPickerTest, BoundedCacheEvictsOlderRequestsAndRejectsOversizedGeometry)
{
    CpuViewportPicker small(2048, 1);
    scene->instances = {Instance(PickMesh())};
    EXPECT_EQ(Resolve(small).status, ViewportPickStatus::HIT);
    ++request.context.request;
    scene->instances = {Instance(PickMesh({{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}, 2))};
    EXPECT_EQ(Resolve(small).status, ViewportPickStatus::HIT);
    EXPECT_EQ(small.Statistics().evictions, 1u);
    EXPECT_LE(small.Statistics().bytes + small.Statistics().reservedBytes, 2048u);
    CpuViewportPicker tiny(16);
    EXPECT_EQ(Resolve(tiny).status, ViewportPickStatus::ERROR);
}

TEST_F(CpuViewportPickerTest, StaleDocumentPreparationIsDiscarded)
{
    scene->instances = {Instance(PickMesh())};
    scene->revision  = 1;
    picker.Prepare(scene, request.context);
    ASSERT_EQ(picker.Query(request).status, ViewportPickStatus::PENDING);
    picker.Prepare(scene, request.context);
    auto empty      = std::make_shared<SceneGeometrySnapshot>();
    empty->revision = 2;
    ++request.context.document;
    ++request.context.scene;
    for (int i = 0; i < 100 && picker.Statistics().reservedBytes; ++i) {
        picker.Prepare(empty, request.context);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(picker.Statistics().builds, 0u);
    EXPECT_EQ(picker.Statistics().discarded, 1u);
    EXPECT_EQ(picker.Query(request).status, ViewportPickStatus::MISS);
}

TEST_F(CpuViewportPickerTest, ANewRequestCanRecoverFromWorkingSetBudgetFailure)
{
    CpuViewportPicker small(2048, 1);
    auto              hole   = PickMesh({{-.9f, -1, 0}, {1, -1, 0}, {1, .9f, 0}}, 1, 1);
    auto              target = PickMesh({{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}, 1, 2);
    scene->instances         = {Instance(hole, 1, 100), Instance(target, 0, 101)};
    EXPECT_EQ(Resolve(small).status, ViewportPickStatus::ERROR);
    ++request.context.request;
    scene->instances = {Instance(target)};
    EXPECT_EQ(Resolve(small).status, ViewportPickStatus::HIT);
}

TEST_F(CpuViewportPickerTest, SingularOffRayBoundsDoNotBlockAndResetDrainsWorker)
{
    scene->instances = {Instance(PickMesh())};
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    auto singular        = Instance({}, 0, 2);
    singular.bounds      = AABB{{-1, -1, -1}, {1, 1, 1}};
    singular.world.v[0]  = 0;
    singular.world.v[12] = 10;
    scene->instances.push_back(singular);
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    scene->instances[1].world.v[3] = 1;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
    scene->instances[1].world.v[3] = 0;
    picker.Reset();
    EXPECT_EQ(picker.Statistics().bytes, 0u);
    EXPECT_EQ(picker.Statistics().reservedBytes, 0u);
    EXPECT_EQ(picker.Context().document, 0u);
    scene->instances.resize(1);
    picker.Prepare(scene, request.context);
    ASSERT_EQ(picker.Query(request).status, ViewportPickStatus::PENDING);
    picker.Prepare(scene, request.context);
    picker.Reset();
    EXPECT_EQ(picker.Statistics().reservedBytes, 0u);
}

TEST_F(CpuViewportPickerTest, RecordedColdWarmWorkload)
{
    std::vector<Vector3> vertices;
    for (int y = -50; y < 50; ++y)
        for (int x = -50; x < 50; ++x) {
            const float a = float(x) * .01f, b = float(y) * .01f;
            vertices.insert(vertices.end(),
                            {{a, b, 0}, {a + .01f, b, 0}, {a, b + .01f, 0}, {a + .01f, b, 0}, {a + .01f, b + .01f, 0}, {a, b + .01f, 0}});
        }
    const auto mesh = PickMesh(std::move(vertices));
    for (uint32_t i = 0; i < 200; ++i) {
        auto instance        = Instance(mesh, -float(i % 5), i);
        instance.world.v[12] = float(i % 20);
        scene->instances.push_back(instance);
    }
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    double              total = 0, maximum = 0;
    std::vector<double> samples;
    for (int i = 0; i < 1000; ++i) {
        double ms = 0;
        ASSERT_EQ(MeasureWarmQuery(ms).status, ViewportPickStatus::HIT);
        samples.push_back(ms);
        total += ms;
        maximum = std::max(maximum, ms);
    }
    const auto stats = picker.Statistics();
    RecordProperty("triangles", int(stats.triangles));
    RecordProperty("instances", 200);
    RecordProperty("cache_bytes", std::to_string(stats.bytes));
    RecordProperty("cold_build_ms", std::to_string(stats.preparationMilliseconds));
    RecordProperty("warm_mean_ms", std::to_string(total / 1000));
    RecordProperty("warm_max_ms", std::to_string(maximum));
    std::sort(samples.begin(), samples.end());
    RecordProperty("warm_p50_ms", std::to_string(samples[500]));
    RecordProperty("warm_p95_ms", std::to_string(samples[950]));
    RecordProperty("decode_ms", std::to_string(stats.decodeMilliseconds));
    RecordProperty("bounds_ms", std::to_string(stats.boundsMilliseconds));
    RecordProperty("partition_ms", std::to_string(stats.partitionMilliseconds));
    RecordProperty("allocation_copy_ms", std::to_string(stats.allocationMilliseconds));
    RecordProperty("publication_ms", std::to_string(stats.publicationMilliseconds));
    RecordProperty("peak_mesh_bytes", std::to_string(stats.peakMeshBytes));
    EXPECT_EQ(stats.builds, 1u);
    EXPECT_EQ(stats.triangles, 20000u);
}

TEST(ViewportPickRequest, LogicalCoordinatesMapOnceToRenderedPixels)
{
    EditorCamera        camera;
    ViewportPickRequest request;
    ASSERT_TRUE(MakeViewportPickRequest(camera, {125.5f, 75.25f}, {400, 200}, 1200, 600, {}, request));
    EXPECT_EQ(request.view.pixelX, 376u);
    EXPECT_EQ(request.view.pixelY, 225u);
    EXPECT_TRUE(SameViewportPickRequest(request, request));
    auto changed = request;
    changed.view.imageWidth++;
    EXPECT_FALSE(SameViewportPickRequest(request, changed));
    EXPECT_FALSE(MakeViewportPickRequest(camera, {400, 50}, {400, 200}, 400, 200, {}, request));
    EXPECT_FALSE(MakeViewportPickRequest(camera, {-1, 50}, {400, 200}, 400, 200, {}, request));
    EXPECT_FALSE(MakeViewportPickRequest(camera, {NAN, 50}, {400, 200}, 400, 200, {}, request));
}
TEST_F(CpuViewportPickerTest, FilteredAndLockedFrontSurfacesStillOcclude)
{
    const auto front = Instance(PickMesh(), 2, 20), back = Instance(PickMesh(), 0, 21);
    scene->instances = {front, back};
    request.include  = {back.actor};
    auto result      = Resolve();
    EXPECT_EQ(result.status, ViewportPickStatus::MISS);
    EXPECT_EQ(result.reason, ViewportPickReason::OCCLUDED);
    request.include.clear();
    request.locked = {front.actor};
    EXPECT_EQ(Resolve().reason, ViewportPickReason::OCCLUDED);
    request.ignore = {front.actor};
    EXPECT_EQ(Resolve().actor, back.actor);
    request.policy = ViewportPickPolicy::VISIBLE_SURFACE;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
}
TEST_F(CpuViewportPickerTest, EqualDistanceIsIndependentOfSceneOrder)
{
    const auto a = Instance(PickMesh(), 0, 20), b = Instance(PickMesh(), 0.000001f, 21);
    scene->instances    = {a, b};
    const auto expected = std::min(a.actor, b.actor);
    EXPECT_EQ(Resolve().actor, expected);
    std::reverse(scene->instances.begin(), scene->instances.end());
    EXPECT_EQ(Resolve().actor, expected);
}
TEST_F(CpuViewportPickerTest, SubmitReplaceCancelAndCapacityAreExplicit)
{
    scene->version = scene->revision = 1;
    picker.Prepare(scene, request.context);
    request.context.providers = picker.Context().providers;
    EXPECT_EQ(picker.Submit(request).status, ViewportPickStatus::PENDING);
    EXPECT_EQ(picker.Submit(request).status, ViewportPickStatus::PENDING);
    auto different      = request;
    different.direction = {1, 0, 0};
    EXPECT_EQ(picker.Submit(different).reason, ViewportPickReason::INVALID_REQUEST);
    const auto old = request.context;
    ++request.context.request;
    picker.Submit(request);
    EXPECT_EQ(picker.Poll(old).reason, ViewportPickReason::STALE_REQUEST);
    EXPECT_EQ(picker.Poll(request.context).status, ViewportPickStatus::MISS);
    picker.Cancel(request.context);
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
    std::vector<ViewportPickContext> tokens;
    for (uint64_t i = 0; i < 8; ++i) {
        request.context.surface = i;
        EXPECT_EQ(picker.Submit(request).status, ViewportPickStatus::PENDING);
        tokens.push_back(request.context);
    }
    request.context.surface = 9;
    EXPECT_EQ(picker.Submit(request).reason, ViewportPickReason::RESOURCE_LIMIT);
    std::set<uint64_t> completed;
    for (int tick = 0; tick < 16; ++tick) {
        picker.Prepare(scene, request.context);
        for (auto token : tokens)
            if (picker.Poll(token).status == ViewportPickStatus::MISS)
                completed.insert(token.surface);
    }
    EXPECT_EQ(completed.size(), 8u);
}
TEST_F(CpuViewportPickerTest, VersionedSparseUpdatesAndLostDeltaResynchronize)
{
    const auto mesh = PickMesh();
    for (uint32_t i = 0; i < 10000; ++i) {
        auto instance        = Instance(mesh, 0, i + 1);
        instance.world.v[12] = float(i % 100) * 4;
        instance.world.v[13] = float(i / 100) * 4;
        scene->instances.push_back(instance);
    }
    scene->version = 1;
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    const auto baseline = picker.Statistics();
    ++request.context.camera;
    picker.Prepare(scene, request.context);
    EXPECT_EQ(picker.Statistics().inverseUpdates, baseline.inverseUpdates);
    EXPECT_EQ(picker.Statistics().indexUpdates, baseline.indexUpdates);
    EXPECT_EQ(picker.Query(request).status, ViewportPickStatus::HIT);
    EXPECT_LT(picker.Statistics().candidateTests - baseline.candidateTests, 500u);
    scene              = std::make_shared<SceneGeometrySnapshot>(*scene);
    scene->version     = 2;
    scene->baseVersion = 1;
    for (uint32_t i = 0; i < 100; ++i) {
        scene->changed.push_back(i);
        scene->instances[i].world.v[14] = 1;
    }
    ++request.context.scene;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    EXPECT_EQ(picker.Statistics().inverseUpdates - baseline.inverseUpdates, 100u);
    EXPECT_EQ(picker.Statistics().builds, baseline.builds);
    scene              = std::make_shared<SceneGeometrySnapshot>(*scene);
    scene->version     = 5;
    scene->baseVersion = 4;
    scene->changed.clear();
    scene->instances.erase(scene->instances.begin());
    ++request.context.scene;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
}
TEST_F(CpuViewportPickerTest, SurfaceFactsHaveIndependentIdentityAndCullMirroredGeometry)
{
    auto mesh        = PickMesh();
    scene->instances = {Instance(mesh)};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    mesh = std::make_shared<SceneMeshResources>(*mesh);
    mesh->parts[0].surface.revision++;
    mesh->parts[0].surface.cull    = SceneSurfaceCull::BACK;
    scene->instances[0].resources  = mesh;
    scene->instances[0].world.v[0] = -1;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
    EXPECT_EQ(picker.Statistics().builds, 1u);
    mesh->parts[0].surface.positionsPreserved = false;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
}
namespace {
    struct TestPickProvider : IViewportPickProvider {
        uint64_t                                   id       = 1;
        std::shared_ptr<ViewportPickProxySnapshot> snapshot = std::make_shared<ViewportPickProxySnapshot>();
        uint64_t                                   Id() const override
        {
            return id;
        }
        std::shared_ptr<const ViewportPickProxySnapshot> Snapshot() override
        {
            return snapshot;
        }
    };
} // namespace
TEST_F(CpuViewportPickerTest, ProviderDepthOverlayPolicyAndLifetime)
{
    scene->instances             = {Instance(PickMesh(), 1, 10)};
    auto provider                = std::make_shared<TestPickProvider>();
    provider->snapshot->revision = 1;
    provider->snapshot->proxies.push_back(
        {.target = {.actor = Uuid::CreateWithSeed(60), .generation = 1}, .shape = ViewportBillboardProxy{{0, 0, 0}, {24, 24}}});
    auto registration = picker.RegisterProvider(provider);
    ASSERT_TRUE(registration);
    EXPECT_FALSE(picker.RegisterProvider(provider));
    ASSERT_TRUE(MakeViewportPickRequest({}, {100, 100}, {200, 200}, 400, 400, request.context, request));
    EXPECT_EQ(Resolve().actor, scene->instances[0].actor);
    provider->snapshot = std::make_shared<ViewportPickProxySnapshot>(*provider->snapshot);
    ++provider->snapshot->revision;
    provider->snapshot->proxies[0].depth = ViewportProxyDepth::OVERLAY;
    auto result                          = Resolve();
    EXPECT_EQ(result.actor, provider->snapshot->proxies[0].target.actor);
    EXPECT_EQ(result.semantics, ViewportPickSemantics::EDITOR_PROXY);
    EXPECT_FALSE(result.position);
    EXPECT_EQ(picker.Overlay({}, {200, 200}).size(), 1u);
    request.policy = ViewportPickPolicy::GEOMETRY_ONLY;
    EXPECT_EQ(Resolve().actor, scene->instances[0].actor);
    picker.Submit(request);
    registration.scope.reset();
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
    EXPECT_TRUE(picker.Overlay({}, {200, 200}).empty());
    EXPECT_EQ(Resolve().actor, scene->instances[0].actor);
}
TEST_F(CpuViewportPickerTest, ProxyTieUsesPriorityThenStableIdentity)
{
    auto first = std::make_shared<TestPickProvider>(), second = std::make_shared<TestPickProvider>();
    second->id                = 2;
    first->snapshot->revision = second->snapshot->revision = 1;
    const auto target                                      = ViewportPickTarget{.actor = Uuid::CreateWithSeed(80), .generation = 1};
    first->snapshot->proxies.push_back({.target = target, .shape = ViewportTriangleProxy{{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}}});
    second->snapshot->proxies = first->snapshot->proxies;
    auto b = picker.RegisterProvider(second), a = picker.RegisterProvider(first);
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    EXPECT_EQ(Resolve().target.provider, 1u);
    second->snapshot = std::make_shared<ViewportPickProxySnapshot>(*second->snapshot);
    ++second->snapshot->revision;
    second->snapshot->proxies[0].priority = 2;
    EXPECT_EQ(Resolve().target.provider, 2u);
    second->snapshot = std::make_shared<ViewportPickProxySnapshot>(*second->snapshot);
    ++second->snapshot->revision;
    second->snapshot->proxies.push_back(second->snapshot->proxies[0]);
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
}
TEST_F(CpuViewportPickerTest, OverlayPaintOrderMatchesPickingAcrossPriorityAndStableTies)
{
    auto first = std::make_shared<TestPickProvider>(), second = std::make_shared<TestPickProvider>();
    second->id                = 2;
    first->snapshot->revision = second->snapshot->revision = 1;
    const ViewportPickTarget target{.actor = Uuid::CreateWithSeed(60), .generation = 1};
    first->snapshot->proxies.push_back({.target   = target,
                                        .shape    = ViewportBillboardProxy{{.15f, 0, -.5f}, {24, 24}},
                                        .depth    = ViewportProxyDepth::OVERLAY,
                                        .priority = 10,
                                        .color    = 0xff0000ff});
    second->snapshot->proxies.push_back({.target   = target,
                                         .shape    = ViewportBillboardProxy{{0, 0, 0}, {24, 24}},
                                         .depth    = ViewportProxyDepth::OVERLAY,
                                         .priority = 0,
                                         .color    = 0xffff0000});
    auto secondRegistration = picker.RegisterProvider(second), firstRegistration = picker.RegisterProvider(first);
    ASSERT_TRUE(firstRegistration);
    ASSERT_TRUE(secondRegistration);
    ASSERT_TRUE(MakeViewportPickRequest({}, {100, 100}, {200, 200}, 400, 400, request.context, request));
    const auto verify = [&](uint64_t provider, uint32_t color) {
        const auto result = Resolve();
        ASSERT_EQ(result.status, ViewportPickStatus::HIT);
        EXPECT_EQ(result.target.provider, provider);
        const auto layouts = picker.Overlay({}, {200, 200});
        ASSERT_EQ(layouts.size(), 2u);
        EXPECT_EQ(layouts.back().color, color);
    };
    verify(first->id, 0xff0000ff);
    first->snapshot = std::make_shared<ViewportPickProxySnapshot>(*first->snapshot);
    ++first->snapshot->revision;
    first->snapshot->proxies[0].priority = 0;
    verify(first->id, 0xff0000ff);
    second->snapshot = std::make_shared<ViewportPickProxySnapshot>(*second->snapshot);
    ++second->snapshot->revision;
    second->snapshot->proxies[0].priority = 20;
    verify(second->id, 0xffff0000);
    request.locked.push_back(target.actor);
    const auto locked = Resolve();
    EXPECT_EQ(locked.status, ViewportPickStatus::MISS);
    EXPECT_EQ(locked.reason, ViewportPickReason::OCCLUDED);
}
TEST(ViewportPickProxy, SegmentClipsNearPlaneAndSharesLogicalVisualLayout)
{
    ViewportPickRequest request;
    ASSERT_TRUE(MakeViewportPickRequest({}, {100, 100}, {200, 200}, 400, 400, {}, request));
    ViewportPickProxy   proxy{.shape = ViewportSegmentProxy{{0, 0, 5.5f}, {0, 0, 0}, 4, 2}};
    ViewportProxyLayout layout;
    EXPECT_TRUE(ProjectViewportPickProxy(proxy, request.view.camera, request.view.extent, layout));
    float                  distance = 0, screen = 0;
    std::optional<Vector3> position;
    EXPECT_TRUE(HitViewportPickProxy(proxy, request, distance, screen, position));
    EXPECT_TRUE(position);
    request.view.point = {110, 100};
    EXPECT_FALSE(HitViewportPickProxy(proxy, request, distance, screen, position));
    proxy.shape = ViewportBillboardProxy{{0, 0, 6}, {24, 24}};
    EXPECT_FALSE(ProjectViewportPickProxy(proxy, request.view.camera, request.view.extent, layout));
}
TEST(ViewportPickIndex, IncrementalTreeMatchesIndependentExhaustiveOracle)
{
    ViewportPickIndex                     index;
    std::mt19937                          random(312);
    std::uniform_real_distribution<float> coordinate(-20, 20);
    std::vector<AABB>                     bounds;
    std::vector<uint32_t>                 leaves;
    for (uint32_t i = 0; i < 4096; ++i) {
        const Vector3 point{coordinate(random), coordinate(random), coordinate(random)};
        bounds.push_back({point, point + Vector3{1}});
        leaves.push_back(index.Insert(i, bounds.back()));
    }
    for (uint32_t iteration = 0; iteration < 3; ++iteration) {
        for (uint32_t ray = 0; ray < 256; ++ray) {
            const Vector3      origin{coordinate(random), coordinate(random), 40};
            std::set<uint32_t> expected, actual;
            for (uint32_t i = 0; i < bounds.size(); ++i)
                if (leaves[i] != ViewportPickIndex::NONE && origin.x >= bounds[i].min.x && origin.x <= bounds[i].max.x &&
                    origin.y >= bounds[i].min.y && origin.y <= bounds[i].max.y)
                    expected.insert(i);
            ViewportPickIndex::Cursor cursor;
            uint64_t                  visits = 0;
            while (!cursor.done) {
                uint32_t work = 3, value = 0;
                float    entry = 0;
                if (index.Next(origin, {0, 0, -1}, 0, 80, cursor, work, value, entry, visits))
                    actual.insert(value);
            }
            EXPECT_EQ(actual, expected);
        }
        for (uint32_t i = 0; i < bounds.size(); ++i) {
            if (leaves[i] == ViewportPickIndex::NONE) {
                leaves[i] = index.Insert(i, bounds[i]);
                continue;
            }
            if (i % 3 == 0) {
                index.Remove(leaves[i]);
                leaves[i] = ViewportPickIndex::NONE;
            } else {
                bounds[i].min.x += 2;
                bounds[i].max.x += 2;
                index.Update(leaves[i], bounds[i]);
            }
        }
    }
}

TEST_F(CpuViewportPickerTest, RecordedLargeSceneAndOverlapWorkloads)
{
    const auto mesh = PickMesh();
    for (uint32_t count : {10000u, 100000u}) {
        picker.Reset();
        scene          = std::make_shared<SceneGeometrySnapshot>();
        scene->version = 1;
        for (uint32_t i = 0; i < count; ++i) {
            auto instance        = Instance(mesh, 0, i + 1);
            instance.world.v[12] = float(i % 1000) * 4;
            instance.world.v[13] = float(i / 1000) * 4;
            scene->instances.push_back(instance);
        }
        const auto result = Resolve(picker, std::chrono::seconds(60));
        const auto prefix = std::to_string(count) + "_";
        RecordProperty(prefix + "index_bytes", std::to_string(picker.Statistics().indexBytes));
        if (count == 100000) {
            EXPECT_EQ(result.reason, ViewportPickReason::RESOURCE_LIMIT);
            continue;
        }
        ASSERT_EQ(result.status, ViewportPickStatus::HIT);
        const auto          before = picker.Statistics();
        std::vector<double> samples;
        for (uint32_t i = 0; i < 1000; ++i) {
            request.origin.x    = float(i % 1000) * 4;
            double milliseconds = 0;
            ASSERT_EQ(MeasureWarmQuery(milliseconds).status, ViewportPickStatus::HIT);
            samples.push_back(milliseconds);
        }
        std::sort(samples.begin(), samples.end());
        RecordProperty(prefix + "query_p50_ms", std::to_string(samples[500]));
        RecordProperty(prefix + "query_p95_ms", std::to_string(samples[950]));
        RecordProperty(prefix + "query_max_ms", std::to_string(samples.back()));
        RecordProperty(prefix + "leaves_per_query", std::to_string(double(picker.Statistics().candidateTests - before.candidateTests) / 1000));
        const auto          staticIndex = picker.Statistics();
        std::vector<double> cameraTimes;
        for (uint32_t tick = 0; tick < 1000; ++tick) {
            ++request.context.camera;
            const auto start = std::chrono::steady_clock::now();
            picker.Prepare(scene, request.context);
            cameraTimes.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
        EXPECT_EQ(picker.Statistics().indexUpdates, staticIndex.indexUpdates);
        EXPECT_EQ(picker.Statistics().inverseUpdates, staticIndex.inverseUpdates);
        std::sort(cameraTimes.begin(), cameraTimes.end());
        RecordProperty("camera_1000_prepare_p50_ms", std::to_string(cameraTimes[500]));
        RecordProperty("camera_1000_prepare_p95_ms", std::to_string(cameraTimes[950]));
        RecordProperty("camera_1000_prepare_max_ms", std::to_string(cameraTimes.back()));
        std::vector<double> viewportTimes;
        for (uint32_t batch = 0; batch < 100; ++batch) {
            std::array<ViewportPickRequest, 8> queries;
            std::array<bool, 8>                complete{};
            const auto                         start = std::chrono::steady_clock::now();
            for (uint32_t view = 0; view < queries.size(); ++view) {
                queries[view]                 = request;
                queries[view].context.surface = view;
                queries[view].context.request = batch + 10;
                queries[view].origin.x        = float(view) * 4;
                ASSERT_EQ(picker.Submit(queries[view]).status, ViewportPickStatus::PENDING);
            }
            uint32_t remaining = 8;
            for (uint32_t tick = 0; tick < 32 && remaining; ++tick) {
                picker.Prepare(scene, request.context);
                for (uint32_t view = 0; view < queries.size(); ++view) {
                    if (complete[view])
                        continue;
                    const auto result = picker.Poll(queries[view].context);
                    if (result.status == ViewportPickStatus::PENDING)
                        continue;
                    EXPECT_EQ(result.status, ViewportPickStatus::HIT);
                    complete[view] = true;
                    --remaining;
                    viewportTimes.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
                    picker.Cancel(queries[view].context);
                }
            }
            ASSERT_EQ(remaining, 0u);
        }
        std::sort(viewportTimes.begin(), viewportTimes.end());
        RecordProperty("eight_view_800_completion_p50_ms", std::to_string(viewportTimes[400]));
        RecordProperty("eight_view_800_completion_p95_ms", std::to_string(viewportTimes[760]));
        RecordProperty("eight_view_800_completion_max_ms", std::to_string(viewportTimes.back()));
        std::vector<double> updates;
        for (uint32_t tick = 0; tick < 20; ++tick) {
            auto next         = std::make_shared<SceneGeometrySnapshot>(*scene);
            next->baseVersion = scene->version;
            ++next->version;
            next->changed.clear();
            for (uint32_t i = 0; i < 100; ++i) {
                next->instances[i].world.v[14] += .01f;
                next->changed.push_back(i);
            }
            scene = std::move(next);
            ++request.context.scene;
            scene->revision  = request.context.scene;
            const auto begin = std::chrono::steady_clock::now();
            for (uint32_t part = 0; part < 100 && picker.Context().scene != scene->revision; ++part)
                picker.Prepare(scene, request.context);
            // Complete any sliced maintenance without charging frame pacing to CPU maintenance.
            while (picker.Query(request).status == ViewportPickStatus::PENDING)
                picker.Prepare(scene, request.context);
            updates.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
        }
        std::sort(updates.begin(), updates.end());
        RecordProperty(prefix + "sparse_p50_ms", std::to_string(updates[10]));
        RecordProperty(prefix + "sparse_p95_ms", std::to_string(updates[19]));
        EXPECT_EQ(picker.Statistics().builds, 1u);
    }
    picker.Reset();
    scene          = std::make_shared<SceneGeometrySnapshot>();
    scene->version = 1;
    for (uint32_t i = 0; i < 10000; ++i)
        scene->instances.push_back(Instance(mesh, 0, i + 1));
    request.origin = {0, 0, 5};
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    ++request.context.request;
    const auto started = std::chrono::steady_clock::now();
    uint32_t   slices  = 0;
    while (picker.Query(request).status == ViewportPickStatus::PENDING && ++slices < 1000) {
    }
    RecordProperty("overlap_10000_slices", int(slices + 1));
    RecordProperty("overlap_10000_query_ms",
                   std::to_string(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count()));
    EXPECT_LT(slices, 1000u);
}

TEST_F(CpuViewportPickerTest, ProjectionCacheInvalidatesForReadinessAndCameraOnly)
{
    auto provider                = std::make_shared<TestPickProvider>();
    provider->snapshot->revision = 1;
    auto registration            = picker.RegisterProvider(provider);
    ASSERT_TRUE(MakeViewportPickRequest({}, {100, 100}, {200, 200}, 400, 400, request.context, request));
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
    provider->snapshot = std::make_shared<ViewportPickProxySnapshot>(*provider->snapshot);
    provider->snapshot->proxies.push_back({.target = {.actor = Uuid::CreateWithSeed(71), .generation = 1},
                                           .shape  = ViewportBillboardProxy{{0, 0, 0}, {24, 24}},
                                           .depth  = ViewportProxyDepth::OVERLAY});
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
    const auto before = picker.Statistics();
    picker.Overlay({}, {200, 200});
    picker.Overlay({}, {200, 200});
    EXPECT_EQ(picker.Statistics().projectionUpdates, before.projectionUpdates);
    auto camera = EditorCamera{};
    camera.yaw  = .1f;
    picker.Overlay(camera, {200, 200});
    EXPECT_GT(picker.Statistics().projectionUpdates, before.projectionUpdates);
    EXPECT_EQ(picker.Statistics().indexUpdates, before.indexUpdates);
}
TEST_F(CpuViewportPickerTest, UnknownBoundsAndDuplicateInstancesAreNeverSilentlySkipped)
{
    scene->instances          = {Instance(PickMesh(), 0, 1), Instance({}, 0, 2)};
    scene->instances[1].state = SceneGeometryState::UNSUPPORTED;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
    scene->instances[1] = scene->instances[0];
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
}
TEST_F(CpuViewportPickerTest, RandomTransformedTrianglesMatchWorldSpaceOracle)
{
    std::mt19937                          random(859);
    std::uniform_real_distribution<float> coordinate(-8, 8), scale(.2f, 2.f);
    auto                                  mesh = PickMesh();
    for (uint32_t i = 0; i < 128; ++i) {
        auto instance        = Instance(mesh, coordinate(random), i + 1);
        instance.world.v[0]  = scale(random) * (i % 2 ? -1.f : 1.f);
        instance.world.v[5]  = scale(random);
        instance.world.v[10] = scale(random);
        instance.world.v[12] = coordinate(random);
        instance.world.v[13] = coordinate(random);
        scene->instances.push_back(instance);
    }
    scene->version = 1;
    Resolve();
    for (uint32_t ray = 0; ray < 512; ++ray) {
        request.origin      = {coordinate(random), coordinate(random), 12};
        request.farDistance = 30;
        Uuid  expected;
        float nearest = 30;
        for (const auto &instance : scene->instances) {
            const auto  &m = instance.world;
            const double x = (request.origin.x - m.v[12]) / m.v[0], y = (request.origin.y - m.v[13]) / m.v[5];
            const float  distance = request.origin.z - m.v[14];
            if (y >= -1 && y <= 1 && std::abs(x) <= (1 - y) * .5 && distance >= request.nearDistance && distance < nearest) {
                nearest  = distance;
                expected = instance.actor;
            }
        }
        const auto result = picker.Query(request);
        EXPECT_EQ(result.status, expected ? ViewportPickStatus::HIT : ViewportPickStatus::MISS);
        EXPECT_EQ(result.actor, expected);
    }
}

TEST_F(CpuViewportPickerTest, ProviderUnregistrationReleasesForeignObjectsBeforeLease)
{
    struct ProbeProvider : IViewportPickProvider {
        std::weak_ptr<int> lease;
        bool              &destroyed;
        explicit ProbeProvider(bool &value) : destroyed(value)
        {
        }
        ~ProbeProvider() override
        {
            EXPECT_FALSE(lease.expired());
            destroyed = true;
        }
        uint64_t Id() const override
        {
            return 71;
        }
        std::shared_ptr<const ViewportPickProxySnapshot> Snapshot() override
        {
            auto result      = std::make_shared<ViewportPickProxySnapshot>();
            result->revision = 1;
            return result;
        }
    };
    bool                     destroyed = false;
    auto                     lease     = std::make_shared<int>(1);
    const std::weak_ptr<int> weak      = lease;
    auto                     provider  = std::make_shared<ProbeProvider>(destroyed);
    provider->lease                    = lease;
    auto scope                         = picker.RegisterProvider(provider, lease);
    provider.reset();
    lease.reset();
    EXPECT_FALSE(weak.expired());
    scope.scope.reset();
    EXPECT_TRUE(destroyed);
    EXPECT_TRUE(weak.expired());
}
TEST_F(CpuViewportPickerTest, ProviderCapacityAndInvalidViewFailExplicitly)
{
    auto provider                = std::make_shared<TestPickProvider>();
    provider->snapshot->revision = 1;
    provider->snapshot->proxies.resize(4097);
    auto scope = picker.RegisterProvider(provider);
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
    scope.scope.reset();
    ASSERT_TRUE(MakeViewportPickRequest({}, {100, 100}, {200, 200}, 400, 400, request.context, request));
    ++request.view.pixelX;
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
}

namespace {
    struct GeometryProbe {
        uint32_t calls = 0, created = 0, destroyed = 0;
        uint32_t geometriesDestroyed = 0;
        bool     validBudget         = true;
    };
    class PlanePickGeometry final : public IViewportPickGeometry {
        struct Cursor final : IViewportPickGeometryCursor {
            Cursor(std::shared_ptr<GeometryProbe> value, uint32_t steps) : probe(std::move(value)), steps(steps)
            {
                ++probe->created;
            }
            ~Cursor() override
            {
                ++probe->destroyed;
            }
            std::shared_ptr<GeometryProbe> probe;
            uint32_t                       steps;
        };

    public:
        ~PlanePickGeometry() override
        {
            ++probe->geometriesDestroyed;
        }
        std::shared_ptr<GeometryProbe> probe           = std::make_shared<GeometryProbe>();
        float                          z               = 0;
        uint32_t                       steps           = 1;
        ViewportPickStatus             status          = ViewportPickStatus::HIT;
        bool                           invalidDistance = false, invalidNormal = false, invalidBudget = false;
        ViewportPickGeometryResult
        Raycast(const ViewportPickRay &ray, ViewportPickGeometryBudget &budget, std::unique_ptr<IViewportPickGeometryCursor> &cursor) const override
        {
            ++probe->calls;
            probe->validBudget &= budget.work > 0 && budget.work <= 256 && budget.milliseconds > 0 && budget.milliseconds <= 1;
            if (invalidBudget) {
                ++budget.work;
                return {.status = ViewportPickStatus::MISS};
            }
            --budget.work;
            if (!cursor)
                cursor = std::make_unique<Cursor>(probe, steps);
            auto &state = static_cast<Cursor &>(*cursor);
            if (--state.steps)
                return {};
            if (status != ViewportPickStatus::HIT)
                return {.status = status, .reason = ViewportPickReason::UNSUPPORTED_COVERAGE, .message = "Custom surface unavailable"};
            const float distance = (z - ray.origin.z) / ray.direction.z;
            if (!std::isfinite(distance) || distance < ray.nearDistance || distance > ray.farDistance)
                return {.status = ViewportPickStatus::MISS};
            return {.status     = ViewportPickStatus::HIT,
                    .distance   = invalidDistance ? NAN : distance,
                    .position   = ray.origin + ray.direction * distance,
                    .normal     = Vector3{0, 0, invalidNormal ? 2.f : 1.f},
                    .instance   = 7,
                    .part       = 3,
                    .subelement = 11};
        }
    };
    ViewportPickGeometry CustomGeometry(std::shared_ptr<const IViewportPickGeometry> query, uint32_t actor = 900)
    {
        return {.target   = {.actor = Uuid::CreateWithSeed(actor), .generation = 1},
                .bounds   = AABB{{-1, -1, -10}, {1, 1, 4}},
                .geometry = std::move(query)};
    }
} // namespace

TEST_F(CpuViewportPickerTest, CustomGeometrySharesWorldResolutionFiltersAndBounds)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    query->z                       = 2;
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry(query)};
    auto offRay                    = std::make_shared<PlanePickGeometry>();
    offRay->status                 = ViewportPickStatus::ERROR;
    auto outside                   = CustomGeometry(offRay, 901);
    outside.bounds                 = AABB{{10, 10, 0}, {11, 11, 1}};
    provider->snapshot->geometries.push_back(outside);
    auto registration = picker.RegisterProvider(provider);
    scene->instances  = {Instance(PickMesh())};
    const auto hit    = Resolve();
    EXPECT_EQ(hit.status, ViewportPickStatus::HIT);
    EXPECT_EQ(hit.actor, provider->snapshot->geometries[0].target.actor);
    EXPECT_EQ(hit.semantics, ViewportPickSemantics::GEOMETRY_SURFACE);
    EXPECT_EQ(hit.target.instance, 7u);
    EXPECT_EQ(hit.target.part, 3u);
    EXPECT_EQ(hit.target.subelement, 11u);
    EXPECT_FLOAT_EQ(hit.distance, 3.f);
    EXPECT_EQ(offRay->probe->calls, 0u);
    request.locked = {hit.actor};
    EXPECT_EQ(Resolve().reason, ViewportPickReason::OCCLUDED);
    request.ignore = {hit.actor};
    EXPECT_EQ(Resolve().actor, scene->instances.front().actor);
    request.ignore.clear();
    request.locked.clear();
    request.policy = ViewportPickPolicy::GEOMETRY_ONLY;
    EXPECT_EQ(Resolve().actor, hit.actor);
    request.policy = ViewportPickPolicy::VISIBLE_SURFACE;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
}

TEST_F(CpuViewportPickerTest, CustomGeometryResumesAndCancellationDestroysCursor)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    query->steps                   = 3;
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry(query)};
    auto registration              = picker.RegisterProvider(provider);
    scene->version = scene->revision = 1;
    picker.Prepare(scene, request.context);
    request.context.providers = picker.Context().providers;
    picker.Submit(request);
    ViewportPickResult result;
    for (int i = 0; i < 20; ++i) {
        picker.Prepare(scene, request.context);
        result = picker.Poll(request.context);
        if (result.status != ViewportPickStatus::PENDING)
            break;
    }
    EXPECT_EQ(result.status, ViewportPickStatus::HIT);
    EXPECT_TRUE(query->probe->validBudget);
    EXPECT_EQ(query->probe->created, query->probe->destroyed);
    EXPECT_EQ(picker.Statistics().builds, 0u);
    ++request.context.request;
    picker.Submit(request);
    picker.Prepare(scene, request.context);
    EXPECT_EQ(picker.Poll(request.context).status, ViewportPickStatus::PENDING);
    EXPECT_GT(query->probe->created, query->probe->destroyed);
    picker.Cancel(request.context);
    EXPECT_EQ(query->probe->created, query->probe->destroyed);
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
}

TEST_F(CpuViewportPickerTest, CustomGeometryRevisionAndUnregisterRetainCursorLease)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    query->steps                   = 3;
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry(query)};
    bool       released            = false;
    const auto probe               = query->probe;
    auto       lease               = std::shared_ptr<const void>(new int(0), [&](const void *value) {
        EXPECT_EQ(probe->created, probe->destroyed);
        EXPECT_EQ(probe->geometriesDestroyed, 1u);
        released = true;
        delete static_cast<const int *>(value);
    });
    auto       registration        = picker.RegisterProvider(provider, lease);
    lease.reset();
    scene->version = scene->revision = 1;
    picker.Prepare(scene, request.context);
    request.context.providers = picker.Context().providers;
    picker.Submit(request);
    ASSERT_EQ(picker.Poll(request.context).status, ViewportPickStatus::PENDING);
    provider->snapshot = std::make_shared<ViewportPickProxySnapshot>(*provider->snapshot);
    ++provider->snapshot->revision;
    picker.Prepare(scene, request.context);
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
    picker.Cancel(request.context);
    request.context.providers = picker.Context().providers;
    ++request.context.request;
    picker.Submit(request);
    ASSERT_EQ(picker.Poll(request.context).status, ViewportPickStatus::PENDING);
    registration.scope.reset();
    provider.reset();
    query.reset();
    picker.Prepare(scene, request.context);
    EXPECT_FALSE(released);
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
    picker.Cancel(request.context);
    EXPECT_TRUE(released);
}

TEST_F(CpuViewportPickerTest, CustomGeometryUnknownCoverageAndMalformedResultsAreExplicit)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry(query)};
    provider->snapshot->geometries.front().bounds.reset();
    auto       registration = picker.RegisterProvider(provider);
    const auto publish      = [&] {
        auto snapshot = std::make_shared<ViewportPickProxySnapshot>(*provider->snapshot);
        ++snapshot->revision;
        snapshot->geometries.front().geometry = std::make_shared<PlanePickGeometry>(*query);
        provider->snapshot                    = std::move(snapshot);
    };
    query->status = ViewportPickStatus::UNSUPPORTED;
    publish();
    scene->instances = {Instance(PickMesh())};
    EXPECT_EQ(Resolve().status, ViewportPickStatus::UNSUPPORTED);
    query->status          = ViewportPickStatus::HIT;
    query->invalidDistance = true;
    publish();
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    query->invalidDistance = false;
    query->invalidNormal   = true;
    publish();
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    query->invalidNormal = false;
    query->invalidBudget = true;
    publish();
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    query->invalidBudget = false;
    publish();
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
}

TEST_F(CpuViewportPickerTest, CustomGeometryPriorityIsStableAgainstMeshAndProviders)
{
    auto query                = std::make_shared<PlanePickGeometry>();
    auto first                = std::make_shared<TestPickProvider>();
    auto second               = std::make_shared<TestPickProvider>();
    second->id                = 2;
    first->snapshot->revision = second->snapshot->revision = 1;
    first->snapshot->geometries                            = {CustomGeometry(query, 900)};
    second->snapshot->geometries                           = {CustomGeometry(query, 901)};
    first->snapshot->geometries[0].priority                = 10;
    second->snapshot->geometries[0].priority               = 20;
    auto b = picker.RegisterProvider(second), a = picker.RegisterProvider(first);
    scene->instances = {Instance(PickMesh())};
    EXPECT_EQ(Resolve().target.provider, 2u);
    b.scope.reset();
    EXPECT_EQ(Resolve().target.provider, 1u);
    a.scope.reset();
    EXPECT_EQ(Resolve().actor, scene->instances[0].actor);
}

TEST_F(CpuViewportPickerTest, NullProviderSnapshotFailsAndRecoversToSameValidOwner)
{
    auto provider = std::make_shared<TestPickProvider>();
    provider->snapshot.reset();
    auto registration = picker.RegisterProvider(provider);
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    provider->snapshot           = std::make_shared<ViewportPickProxySnapshot>();
    provider->snapshot->revision = 1;
    const auto valid             = provider->snapshot;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
    provider->snapshot.reset();
    EXPECT_EQ(Resolve().status, ViewportPickStatus::ERROR);
    provider->snapshot = valid;
    EXPECT_EQ(Resolve().status, ViewportPickStatus::MISS);
}

TEST_F(CpuViewportPickerTest, AsyncMeshBudgetFailureTerminatesWithResourceLimit)
{
    CpuViewportPicker tiny(16);
    scene->revision = scene->version = 1;
    scene->instances                 = {Instance(PickMesh())};
    tiny.Prepare(scene, request.context);
    request.context.providers = tiny.Context().providers;
    tiny.Submit(request);
    ViewportPickResult result;
    for (int i = 0; i < 10; ++i) {
        tiny.Prepare(scene, request.context);
        result = tiny.Poll(request.context);
        if (result.status != ViewportPickStatus::PENDING)
            break;
    }
    EXPECT_EQ(result.status, ViewportPickStatus::ERROR);
    EXPECT_EQ(result.reason, ViewportPickReason::RESOURCE_LIMIT);
}

TEST_F(CpuViewportPickerTest, CustomGeometryMetadataAndCapacityAreValidated)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry({})};
    auto registration              = picker.RegisterProvider(provider);
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    const auto publish = [&](ViewportPickGeometry geometry) {
        auto snapshot      = std::make_shared<ViewportPickProxySnapshot>();
        snapshot->revision = provider->snapshot->revision + 1;
        snapshot->geometries.push_back(std::move(geometry));
        provider->snapshot = std::move(snapshot);
    };
    auto invalid          = CustomGeometry(query);
    invalid.bounds->min.x = NAN;
    publish(invalid);
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    invalid                 = CustomGeometry(query);
    invalid.target.provider = 2;
    publish(invalid);
    EXPECT_EQ(Resolve().reason, ViewportPickReason::INVALID_REQUEST);
    auto oversized      = std::make_shared<ViewportPickProxySnapshot>();
    oversized->revision = provider->snapshot->revision + 1;
    oversized->geometries.resize(4097);
    provider->snapshot = oversized;
    EXPECT_EQ(Resolve().reason, ViewportPickReason::RESOURCE_LIMIT);
    publish(CustomGeometry(query));
    EXPECT_EQ(Resolve().status, ViewportPickStatus::HIT);
}

TEST_F(CpuViewportPickerTest, CustomGeometryNewRequestAndDocumentDropOldContinuation)
{
    auto query                     = std::make_shared<PlanePickGeometry>();
    query->steps                   = 3;
    auto provider                  = std::make_shared<TestPickProvider>();
    provider->snapshot->revision   = 1;
    provider->snapshot->geometries = {CustomGeometry(query)};
    auto registration              = picker.RegisterProvider(provider);
    scene->revision = scene->version = 1;
    picker.Prepare(scene, request.context);
    request.context.providers = picker.Context().providers;
    picker.Submit(request);
    EXPECT_EQ(picker.Poll(request.context).status, ViewportPickStatus::PENDING);
    const auto old = request.context;
    ++request.context.request;
    picker.Submit(request);
    EXPECT_EQ(query->probe->created, query->probe->destroyed);
    EXPECT_EQ(picker.Poll(old).reason, ViewportPickReason::STALE_REQUEST);
    picker.Prepare(scene, request.context);
    EXPECT_EQ(picker.Poll(request.context).status, ViewportPickStatus::PENDING);
    auto nextDocument = request.context;
    ++nextDocument.document;
    picker.Prepare(scene, nextDocument);
    EXPECT_EQ(query->probe->created, query->probe->destroyed);
    EXPECT_EQ(picker.Poll(request.context).reason, ViewportPickReason::STALE_REQUEST);
}

TEST_F(CpuViewportPickerTest, SelectionFeedbackUsesOwnedBoundsAndCustomProviderLifetime)
{
    const auto mesh  = PickMesh();
    scene->instances = {Instance(mesh, 2)};
    ASSERT_EQ(Resolve().status, ViewportPickStatus::HIT);
    const auto actor  = scene->instances.front().actor;
    auto       bounds = picker.SelectionBounds(actor);
    ASSERT_TRUE(bounds.bounds);
    EXPECT_TRUE(bounds.surface);
    EXPECT_FLOAT_EQ(bounds.bounds->min.z, 2);
    auto provider                = std::make_shared<TestPickProvider>();
    provider->id                 = 91;
    provider->snapshot->revision = 1;
    ViewportPickGeometry geometry;
    geometry.target   = {.actor = actor, .generation = 1, .provider = 91};
    geometry.bounds   = AABB{{-2, -2, -1}, {2, 2, 1}};
    geometry.geometry = std::make_shared<PlanePickGeometry>();
    provider->snapshot->geometries.push_back(geometry);
    auto registration = picker.RegisterProvider(provider);
    ASSERT_TRUE(registration);
    picker.Prepare(scene, request.context);
    bounds = picker.SelectionBounds(actor);
    ASSERT_TRUE(bounds.bounds);
    EXPECT_FALSE(bounds.surface);
    EXPECT_FLOAT_EQ(bounds.bounds->min.x, -2);
    registration.scope.reset();
    EXPECT_TRUE(picker.SelectionBounds(actor).surface);
    picker.Reset();
    EXPECT_FALSE(picker.SelectionBounds(actor).bounds);
}
