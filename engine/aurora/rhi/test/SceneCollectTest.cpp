//
// Scene tests: frustum culling, entity lifecycle, view depth (ECS storage).
// Draw-item collection is deferred until the technique design lands.
//

#include "AuroraTestHelper.h"

#include <aurora/scene/RenderScene.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, SceneCollectFrustumCull)
{
    RenderScene scene;
    auto *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    // identity view-project: frustum is unit cube-ish; far-z primitive outside
    AABB inside(Vector3(0.f, 0.f, -0.5f), Vector3(0.f, 0.f, 0.5f));
    AABB farOutside(Vector3(100.f, 100.f, 100.f), Vector3(101.f, 101.f, 101.f));

    EXPECT_TRUE(view->FrustumCulling(inside));
    EXPECT_FALSE(view->FrustumCulling(farOutside));
}

TEST_F(AuroraVulkanTest, SceneEntityLifecycle)
{
    RenderScene scene;
    const EntityId id = scene.CreateEntity();
    EXPECT_TRUE(scene.IsAlive(id));

    scene.Add<Light>(id, Light{});
    scene.Add<Skin>(id, Skin{12});
    EXPECT_NE(scene.Get<Light>(id), nullptr);
    EXPECT_EQ(scene.Get<Skin>(id)->jointCount, 12u);

    scene.DestroyEntity(id);
    EXPECT_FALSE(scene.IsAlive(id));
    EXPECT_EQ(scene.Get<Light>(id), nullptr); // stale generation rejected
}

TEST_F(AuroraVulkanTest, SceneBoundsPoolDense)
{
    RenderScene scene;
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
    auto *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    // view-space depth == z for identity view
    EXPECT_LT(view->ViewSpaceDepth(Vector3(0, 0, -10.f)), view->ViewSpaceDepth(Vector3(0, 0, -1.f)));
}

TEST_F(AuroraVulkanTest, SceneLightPointSpotParams)
{
    RenderScene scene;
    const EntityId id = scene.CreateEntity();

    Light spot{};
    spot.type           = LightType::SPOT;
    spot.color          = Vector3(1.f, 0.9f, 0.8f);
    spot.intensity      = 3.f;
    spot.position       = Vector3(0.f, 5.f, 0.f);
    spot.direction      = Vector3(0.f, -1.f, 0.f);
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
    EXPECT_FLOAT_EQ(stored->position.y, 5.f);
}

TEST_F(AuroraVulkanTest, SceneWorldInfoMatrixStorage)
{
    RenderScene scene;
    const EntityId id = scene.CreateEntity();

    Matrix4 m = Matrix4::Identity();
    m.m[3] = Vector4(10.f, 20.f, 0.f, 1.f); // translation column
    scene.Add<WorldInfo>(id, WorldInfo{m});

    const auto *stored = scene.Get<WorldInfo>(id);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->world.m[3].x, 10.f);
    EXPECT_FLOAT_EQ(stored->world.m[3].y, 20.f);
}

TEST_F(AuroraVulkanTest, SceneViewLightAndWorldInfo)
{
    RenderScene scene;
    const EntityId both = scene.CreateEntity();
    const EntityId lightOnly = scene.CreateEntity();

    scene.Add<Light>(both, Light{});
    scene.Add<WorldInfo>(both, WorldInfo{});
    scene.Add<Light>(lightOnly, Light{});

    int hits = 0;
    scene.GetRegistry().View<Light, WorldInfo>().ForEach(
        [&](EntityId id, Light &, WorldInfo &) {
            EXPECT_EQ(id, both);
            ++hits;
        });
    EXPECT_EQ(hits, 1);
}
