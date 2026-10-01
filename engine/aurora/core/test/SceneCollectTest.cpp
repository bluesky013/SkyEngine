//
// Scene tests: frustum culling, entity lifecycle, view depth (ECS storage).
// Draw-item collection is deferred until the technique design lands.
//

#include "AuroraTestHelper.h"

#include <aurora/light/LightShell.h>
#include <aurora/light/LightSystem.h>
#include <aurora/scene/RenderScene.h>
#include <core/math/Quaternion.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, SceneCollectFrustumCull)
{
    RenderScene scene;
    auto       *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    // identity view-project: frustum is unit cube-ish; far-z primitive outside
    BoundingBoxSphere inside     = BoundingBoxSphere::FromMinMax(Vector3(0.f, 0.f, -0.5f), Vector3(0.f, 0.f, 0.5f));
    BoundingBoxSphere farOutside = BoundingBoxSphere::FromMinMax(Vector3(100.f, 100.f, 100.f), Vector3(101.f, 101.f, 101.f));

    EXPECT_TRUE(view->FrustumCulling(inside));
    EXPECT_FALSE(view->FrustumCulling(farOutside));
}

TEST_F(AuroraVulkanTest, SceneEntityLifecycle)
{
    RenderScene    scene;
    const EntityId id = scene.CreateEntity();
    EXPECT_TRUE(scene.IsAlive(id));

    scene.Add<Light>(id, Light{});
    scene.Add<SkinnedMesh>(id, SkinnedMesh{});
    EXPECT_NE(scene.Get<Light>(id), nullptr);
    EXPECT_NE(scene.Get<SkinnedMesh>(id), nullptr);

    scene.DestroyEntity(id);
    EXPECT_FALSE(scene.IsAlive(id));
    EXPECT_EQ(scene.Get<Light>(id), nullptr); // stale generation rejected
}

TEST_F(AuroraVulkanTest, SceneBoundsPoolDense)
{
    RenderScene    scene;
    const EntityId e1 = scene.CreateEntity();
    const EntityId e2 = scene.CreateEntity();
    const EntityId e3 = scene.CreateEntity();

    scene.Add<Bounds>(e1, Bounds{});
    scene.Add<Bounds>(e2, Bounds{});
    scene.Add<Bounds>(e3, Bounds{});

    auto &pool = scene.Pool<Bounds>();
    EXPECT_EQ(pool.Size(), 3u);

    scene.DestroyEntity(e2);
    pool.Remove(e2);
    EXPECT_EQ(pool.Size(), 2u);
    EXPECT_EQ(pool.Get(e2), nullptr);
    EXPECT_NE(pool.Get(e1), nullptr);
    EXPECT_NE(pool.Get(e3), nullptr);
}

TEST_F(AuroraVulkanTest, SceneViewDepth)
{
    RenderScene scene;
    auto       *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    // view-space depth == z for identity view
    EXPECT_LT(view->ViewSpaceDepth(Vector3(0, 0, -10.f)), view->ViewSpaceDepth(Vector3(0, 0, -1.f)));
}

TEST_F(AuroraVulkanTest, SceneLightPointSpotParams)
{
    RenderScene    scene;
    const EntityId id = scene.CreateEntity();

    Light spot{};
    spot.type           = LightType::SPOT;
    spot.color          = Vector3(1.f, 0.9f, 0.8f);
    spot.intensity      = 3.f;
    spot.range          = 25.f;
    spot.innerConeAngle = 0.3f;
    spot.outerConeAngle = 0.6f;
    scene.Add<Light>(id, spot);

    const auto *stored = scene.Get<Light>(id);
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->type, LightType::SPOT);
    EXPECT_FLOAT_EQ(stored->intensity, 3.f);
    EXPECT_FLOAT_EQ(stored->range, 25.f);
    EXPECT_FLOAT_EQ(stored->innerConeAngle, 0.3f);
    EXPECT_FLOAT_EQ(stored->outerConeAngle, 0.6f);
}

TEST_F(AuroraVulkanTest, SceneWorldInfoMatrixStorage)
{
    RenderScene    scene;
    const EntityId id = scene.CreateEntity();

    Matrix4 m = Matrix4::Identity();
    m.m[3]    = Vector4(10.f, 20.f, 0.f, 1.f); // translation column
    scene.Add<WorldInfo>(id, WorldInfo{m});

    const auto *stored = scene.Get<WorldInfo>(id);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->world.m[3].x, 10.f);
    EXPECT_FLOAT_EQ(stored->world.m[3].y, 20.f);
}

TEST_F(AuroraVulkanTest, SceneViewLightAndWorldInfo)
{
    RenderScene    scene;
    const EntityId both      = scene.CreateEntity();
    const EntityId lightOnly = scene.CreateEntity();

    scene.Add<Light>(both, Light{});
    scene.Add<WorldInfo>(both, WorldInfo{});
    scene.Add<Light>(lightOnly, Light{});

    int hits = 0;
    scene.GetRegistry().View<Light, WorldInfo>().ForEach([&](EntityId id, Light &, WorldInfo &) {
        EXPECT_EQ(id, both);
        ++hits;
    });
    EXPECT_EQ(hits, 1);
}

TEST_F(AuroraVulkanTest, SceneMainLightComponent)
{
    RenderScene    scene;
    const EntityId id = scene.CreateEntity();

    scene.Add<Light>(id, Light{});
    scene.Add<MainLight>(id, MainLight{});
    scene.Add<WorldInfo>(id, WorldInfo{});

    const MainLight *stored = scene.Get<MainLight>(id);
    ASSERT_NE(stored, nullptr);
    EXPECT_TRUE(stored->castShadow);

    int count = 0;
    scene.GetRegistry().View<MainLight, Light, WorldInfo>().ForEach([&](EntityId, MainLight &, Light &, WorldInfo &) { ++count; });
    EXPECT_EQ(count, 1);
}

TEST_F(AuroraVulkanTest, SceneLocalLightSystemGathersRenderData)
{
    RenderScene    scene;
    const EntityId point = scene.CreateEntity();
    const EntityId spot  = scene.CreateEntity();

    WorldInfo pointWorld;
    pointWorld.world.m[3] = Vector4(1.f, 2.f, 3.f, 1.f);

    Light pointLight{};
    pointLight.type      = LightType::POINT;
    pointLight.color     = Vector3(1.f, 0.5f, 0.25f);
    pointLight.intensity = 2.f;
    pointLight.range     = 8.f;
    scene.Add<Light>(point, pointLight);
    scene.Add<WorldInfo>(point, pointWorld);

    Light spotLight{};
    spotLight.type           = LightType::SPOT;
    spotLight.range          = 12.f;
    spotLight.innerConeAngle = 0.2f;
    spotLight.outerConeAngle = 0.5f;
    scene.Add<Light>(spot, spotLight);
    scene.Add<WorldInfo>(spot, WorldInfo{}); // identity -> derived direction -Z

    std::vector<LightRenderData> out;
    LocalLightSystem::Process(scene, out);
    ASSERT_EQ(out.size(), 2u);

    const LightRenderData &p = out[0];
    EXPECT_FLOAT_EQ(p.position.x, 1.f);
    EXPECT_FLOAT_EQ(p.position.y, 2.f);
    EXPECT_FLOAT_EQ(p.position.z, 3.f);
    EXPECT_FLOAT_EQ(p.position.w, static_cast<float>(LightType::POINT));
    EXPECT_FLOAT_EQ(p.color.w, 2.f);
    EXPECT_FLOAT_EQ(p.params.x, 8.f);

    const LightRenderData &s = out[1];
    EXPECT_FLOAT_EQ(s.position.w, static_cast<float>(LightType::SPOT));
    EXPECT_FLOAT_EQ(s.params.x, 12.f);
    EXPECT_FLOAT_EQ(s.params.y, 0.2f);
    EXPECT_FLOAT_EQ(s.params.z, 0.5f);
    EXPECT_NEAR(s.direction.z, -1.f, 1e-5f);

    // caller owns the buffer: a second pass appends
    LocalLightSystem::Process(scene, out);
    EXPECT_EQ(out.size(), 4u);
}

TEST_F(AuroraVulkanTest, SceneLightTransformDerivation)
{
    Matrix4 world = Quaternion(1.57079632679f, Vector3(0.f, 1.f, 0.f)).ToMatrix(); // +90 deg about Y
    world.m[3]    = Vector4(10.f, 20.f, 30.f, 1.f);

    const Vector3 position = ExtractLightPosition(world);
    EXPECT_FLOAT_EQ(position.x, 10.f);
    EXPECT_FLOAT_EQ(position.y, 20.f);
    EXPECT_FLOAT_EQ(position.z, 30.f);

    // local forward (-Z) rotated +90 deg about Y points down -X
    const Vector3 direction = ExtractLightDirection(world);
    EXPECT_NEAR(direction.x, -1.f, 1e-4f);
    EXPECT_NEAR(direction.y, 0.f, 1e-4f);
    EXPECT_NEAR(direction.z, 0.f, 1e-4f);
    EXPECT_NEAR(direction.Length(), 1.f, 1e-4f);

    // identity leaves forward at -Z
    const Vector3 identity = ExtractLightDirection(Matrix4::Identity());
    EXPECT_NEAR(identity.x, 0.f, 1e-6f);
    EXPECT_NEAR(identity.y, 0.f, 1e-6f);
    EXPECT_NEAR(identity.z, -1.f, 1e-6f);
}

TEST_F(AuroraVulkanTest, LightVolumePointSphere)
{
    WorldInfo world;
    world.world.m[3] = Vector4(1.f, 2.f, 3.f, 1.f);

    Light light{};
    light.type  = LightType::POINT;
    light.range = 7.f;

    const LightVolume volume = ComputeLightVolume(light, world, nullptr);
    EXPECT_EQ(volume.type, LightVolumeType::SPHERE);
    EXPECT_FLOAT_EQ(volume.center.x, 1.f);
    EXPECT_FLOAT_EQ(volume.center.y, 2.f);
    EXPECT_FLOAT_EQ(volume.center.z, 3.f);
    EXPECT_FLOAT_EQ(volume.radius, 7.f);
}

TEST_F(AuroraVulkanTest, LightVolumeSpotCone)
{
    WorldInfo world; // identity -> derived direction is -Z

    Light light{};
    light.type           = LightType::SPOT;
    light.range          = 10.f;
    light.outerConeAngle = 0.5f;

    const LightVolume volume = ComputeLightVolume(light, world, nullptr);
    EXPECT_EQ(volume.type, LightVolumeType::CONE);
    EXPECT_FLOAT_EQ(volume.height, 10.f);
    EXPECT_NEAR(volume.baseRadius, 10.f * std::tan(0.5f), 1e-4f);
    EXPECT_NEAR(volume.axis.z, -1.f, 1e-5f);
}

TEST_F(AuroraVulkanTest, LightBoundsSphereAndCone)
{
    WorldInfo origin;

    Light point{};
    point.type           = LightType::POINT;
    point.range          = 5.f;
    const AABB sphereBox = ComputeLightBounds(point, origin);
    EXPECT_FLOAT_EQ(sphereBox.min.x, -5.f);
    EXPECT_FLOAT_EQ(sphereBox.min.y, -5.f);
    EXPECT_FLOAT_EQ(sphereBox.min.z, -5.f);
    EXPECT_FLOAT_EQ(sphereBox.max.x, 5.f);
    EXPECT_FLOAT_EQ(sphereBox.max.y, 5.f);
    EXPECT_FLOAT_EQ(sphereBox.max.z, 5.f);

    // rotate -Z to -Y so the spot points straight down
    Matrix4 world = Quaternion(-1.57079632679f, Vector3(1.f, 0.f, 0.f)).ToMatrix();
    world.m[3]    = Vector4(0.f, 0.f, 0.f, 1.f);
    WorldInfo rotated{world};

    Light spot{};
    spot.type           = LightType::SPOT;
    spot.range          = 4.f;
    spot.outerConeAngle = 0.4636476f; // tan ~= 0.5 -> baseRadius ~= 2

    const AABB coneBox = ComputeLightBounds(spot, rotated);
    EXPECT_LE(coneBox.max.y, 0.f + 1e-4f);  // apex at origin, cone extends -Y
    EXPECT_LE(coneBox.min.y, -4.f + 1e-3f); // reaches the base plane
    EXPECT_LE(coneBox.min.x, -2.f + 1e-2f); // base disk extends +-baseRadius
    EXPECT_GE(coneBox.max.x, 2.f - 1e-2f);
    EXPECT_LE(coneBox.min.z, -2.f + 1e-2f);
    EXPECT_GE(coneBox.max.z, 2.f - 1e-2f);
}

TEST_F(AuroraVulkanTest, LightShellGeometrySphereAndCone)
{
    LightVolume sphere;
    sphere.type   = LightVolumeType::SPHERE;
    sphere.center = Vector3(0.f, 0.f, 0.f);
    sphere.radius = 2.f;

    const GeometryStreams sphereShell = GenerateLightShellGeometry(sphere, 16, 8);
    ASSERT_FALSE(sphereShell.positions.empty());
    for (const auto &p : sphereShell.positions) {
        EXPECT_NEAR(p.Length(), 2.f, 1e-3f);
    }

    LightVolume cone;
    cone.type       = LightVolumeType::CONE;
    cone.apex       = Vector3(0.f, 0.f, 0.f);
    cone.axis       = Vector3(0.f, -1.f, 0.f);
    cone.height     = 4.f;
    cone.baseRadius = 2.f;

    const GeometryStreams coneShell = GenerateLightShellGeometry(cone, 16, 8);
    ASSERT_FALSE(coneShell.positions.empty());

    bool hasApex = false;
    bool hasBase = false;
    for (const auto &p : coneShell.positions) {
        if (p.Length() < 1e-4f) {
            hasApex = true;
        }
        const float radial = std::sqrt(p.x * p.x + p.z * p.z);
        if (std::abs(p.y + 4.f) < 1e-3f && std::abs(radial - 2.f) < 1e-2f) {
            hasBase = true;
        }
    }
    EXPECT_TRUE(hasApex);
    EXPECT_TRUE(hasBase);
}

TEST_F(AuroraVulkanTest, LightConservativeScreenRect)
{
    const Vector2 extent(100.f, 100.f);

    LightVolume sphere;
    sphere.type   = LightVolumeType::SPHERE;
    sphere.center = Vector3(0.f, 0.f, 0.f);
    sphere.radius = 0.5f;
    sphere.bounds = AABB(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));

    const ScreenRect rect = ComputeConservativeScreenRect(sphere, Matrix4::Identity(), extent);
    EXPECT_TRUE(rect.valid);
    EXPECT_GE(rect.minX, 0.f);
    EXPECT_GE(rect.minY, 0.f);
    EXPECT_LE(rect.maxX, extent.x);
    EXPECT_LE(rect.maxY, extent.y);
    EXPECT_LT(rect.minX, rect.maxX);
    EXPECT_LT(rect.minY, rect.maxY);

    LightVolume box;
    box.type                 = LightVolumeType::BOX;
    const ScreenRect boxRect = ComputeConservativeScreenRect(box, Matrix4::Identity(), extent);
    EXPECT_FALSE(boxRect.valid);
    EXPECT_FLOAT_EQ(boxRect.minX, 0.f);
    EXPECT_FLOAT_EQ(boxRect.maxX, extent.x);
    EXPECT_FLOAT_EQ(boxRect.maxY, extent.y);
}

TEST_F(AuroraVulkanTest, LightConservativeTileCoverage)
{
    const Vector2 extent(100.f, 100.f);

    LightVolume sphere;
    sphere.type   = LightVolumeType::SPHERE;
    sphere.center = Vector3(0.f, 0.f, 0.f);
    sphere.radius = 0.5f;
    sphere.bounds = AABB(Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f));

    const LightCoverage coverage = ComputeConservativeCoverage(sphere, Matrix4::Identity(), extent, 16);
    EXPECT_EQ(coverage.tilesX, 7u);
    EXPECT_EQ(coverage.tilesY, 7u);
    EXPECT_FALSE(coverage.tiles.empty());

    // screen center (50,50) is inside the projection -> its tile must be covered
    const uint32_t centerTile = 3u * coverage.tilesX + 3u;
    bool           found      = false;
    for (uint32_t tile : coverage.tiles) {
        if (tile == centerTile) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    LightVolume far;
    far.type   = LightVolumeType::SPHERE;
    far.center = Vector3(100.f, 100.f, 0.f);
    far.radius = 0.5f;
    far.bounds = AABB(Vector3(99.5f, 99.5f, -0.5f), Vector3(100.5f, 100.5f, 0.5f));

    const LightCoverage offscreen = ComputeConservativeCoverage(far, Matrix4::Identity(), extent, 16);
    EXPECT_TRUE(offscreen.tiles.empty());
}
