//
// Pipeline template smoke tests.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/OpaquePass.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, OpaquePassBuildRDG)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    OpaquePass opaque;
    opaque.SetExtent(1280, 720);
    opaque.OnSetup(device);
    opaque.BuildRDG(*graph);

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool found = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type != CompiledPassType::SCENE_RASTER) {
            continue;
        }
        found = true;
        const auto &p = std::get<SceneRasterPayload>(cpass.payload);
        ASSERT_EQ(p.queues.size(), 1u);
        EXPECT_STREQ(std::string(p.queues[0].name.GetStr()).c_str(), "opaque");
        EXPECT_EQ(p.queues[0].sortPolicy, QueueSortPolicy::FRONT_TO_BACK);
        EXPECT_EQ(p.renderArea.width, 1280u);
        EXPECT_EQ(p.renderArea.height, 720u);
    }
    EXPECT_TRUE(found);
}
