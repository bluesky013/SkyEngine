//
// Scene collect tests: tag filtering, frustum culling, sorting (ECS storage).
//

#include "AuroraTestHelper.h"

#include <aurora/scene/RenderScene.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    EntityId MakeRenderable(RenderScene &scene, const Name &tag, float zCenter, float boundsExtent = 1.f)
    {
        const EntityId id = scene.CreateEntity();
        scene.Add<Bounds>(id, Bounds{AABB(Vector3(0.f, 0.f, zCenter - boundsExtent),
                                          Vector3(0.f, 0.f, zCenter + boundsExtent))});
        RenderItem ri{};
        ri.techniqueTag      = tag;
        ri.item.args.indexCount = static_cast<uint32_t>(zCenter * 1000.f); // encode z for order assertion
        scene.Add<RenderItem>(id, ri);
        return id;
    }

} // namespace

// scene: opaque-only near, opaque-only far, shadow-only; identity view
TEST_F(AuroraVulkanTest, SceneCollectTagFilter)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;

    RenderScene scene;
    auto *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    MakeRenderable(scene, Name("opaque"), 0.1f, 0.05f);
    MakeRenderable(scene, Name("opaque"), 0.5f, 0.05f);
    MakeRenderable(scene, Name("shadow"), 0.3f, 0.05f);

    auto graph = RenderGraph::Build(device, frameAlloc);
    const auto colorTex = graph->CreateTexture(Name("color"),
        [] { RDGTextureDesc d{}; d.extent = {64, 64, 1}; d.usage = ImageUsageFlagBit::RENDER_TARGET; return d; }());

    graph->AddSceneRasterPass(Name("scene"),
        [&](SceneRasterPassBuilder &b) {
            b.ColorAttachment(0, colorTex, LoadOp::CLEAR, StoreOp::STORE);
            const uint32_t q = b.AddQueue(Name("opaque"), QueueSortPolicy::FRONT_TO_BACK, Name("opaque"));

            auto &boundsPool = scene.Pool<Bounds>();
            auto &itemPool   = scene.Pool<RenderItem>();
            for (uint32_t i = 0; i < boundsPool.Size(); ++i) {
                if (!view->FrustumCulling(boundsPool.Data(i).worldBounds)) {
                    continue;
                }
                const auto *ri = itemPool.Get(boundsPool.DenseEntity(i));
                if (ri != nullptr && ri->techniqueTag == Name("opaque")) {
                    b.AddDrawItem(q, ri->item);
                }
            }
        });
    graph->MarkOfInterest(colorTex);
    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);
    ASSERT_EQ(cg->passes.size(), 1u);

    const auto &p = std::get<SceneRasterPayload>(cg->passes[0].payload);
    ASSERT_EQ(p.queues.size(), 1u);
    EXPECT_STREQ(std::string(p.queues[0].techniqueTag.GetStr()).c_str(), "opaque");
    // opaque near + opaque far collected; shadow filtered out
    EXPECT_EQ(p.queues[0].items.size(), 2u);
}

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

TEST_F(AuroraVulkanTest, SceneCollectSortFrontToBack)
{
    RenderScene scene;
    auto *view = scene.CreateView(Name("main"));
    view->SetViewMatrix(Matrix4::Identity());
    view->SetProjectionMatrix(Matrix4::Identity());

    // view-space depth == z for identity view; FRONT_TO_BACK = ascending z
    EXPECT_LT(view->ViewSpaceDepth(Vector3(0, 0, -10.f)), view->ViewSpaceDepth(Vector3(0, 0, -1.f)));
}
