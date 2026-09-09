//
// Scene collect tests: tag filtering, frustum culling, sorting.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/scene/RenderScene.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    RenderPrimitive *MakePrimitive(std::vector<std::unique_ptr<RenderPrimitive>> &storage,
                                   const Name &tag, float zCenter, float boundsExtent = 1.f)
    {
        auto prim = std::make_unique<RenderPrimitive>();
        prim->worldBounds = AABB(Vector3(0.f, 0.f, zCenter - boundsExtent),
                                 Vector3(0.f, 0.f, zCenter + boundsExtent));
        prim->args.indexCount = static_cast<uint32_t>(zCenter); // encode z for order assertion
        prim->techniques[tag] = TechniqueBinding{};
        auto *raw = prim.get();
        storage.push_back(std::move(prim));
        return raw;
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

    std::vector<std::unique_ptr<RenderPrimitive>> storage;
    auto *opaqueNear  = MakePrimitive(storage, Name("opaque"), 0.1f, 0.05f);
    auto *opaqueFar   = MakePrimitive(storage, Name("opaque"), 0.5f, 0.05f);
    auto *shadowOnly  = MakePrimitive(storage, Name("shadow"), 0.3f, 0.05f);
    scene.AddPrimitive(opaqueNear);
    scene.AddPrimitive(opaqueFar);
    scene.AddPrimitive(shadowOnly);

    auto graph = RenderGraph::Build(device, frameAlloc);
    const auto colorTex = graph->CreateTexture(Name("color"),
        [] { RDGTextureDesc d{}; d.extent = {64, 64, 1}; d.usage = ImageUsageFlagBit::RENDER_TARGET; return d; }());

    graph->AddSceneRasterPass(Name("scene"),
        [&](SceneRasterPassBuilder &b) {
            b.ColorAttachment(0, colorTex, LoadOp::CLEAR, StoreOp::STORE);
            const uint32_t q = b.AddQueue(Name("opaque"), QueueSortPolicy::FRONT_TO_BACK, Name("opaque"));

            for (const auto *prim : scene.GetPrimitives()) {
                if (!view->FrustumCulling(prim->worldBounds)) {
                    continue;
                }
                GatherContext ctx{};
                ctx.tag  = Name("opaque");
                ctx.view = view;
                prim->GatherRenderItem(ctx);
                if (ctx.gathered) {
                    b.AddDrawItem(q, ctx.item);
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
    // opaqueNear + opaqueFar collected; shadowOnly filtered out
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

TEST_F(AuroraVulkanTest, SceneCollectEmptyTagGathersAll)
{
    std::vector<std::unique_ptr<RenderPrimitive>> storage;
    auto *prim = MakePrimitive(storage, Name("opaque"), 5.f);

    GatherContext ctx{};
    ctx.tag = Name{}; // empty = no filter
    prim->GatherRenderItem(ctx);
    EXPECT_TRUE(ctx.gathered);
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
