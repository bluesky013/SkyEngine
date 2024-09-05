//
// Created by blues on 2024/9/3.
//

#include <render/RenderPassPipeline.h>

namespace sky {

    void RenderPassPipeline::AddScenePass(RenderScenePipeline *ppl)
    {
        scenePipelines.emplace_back(ppl);
    }

    bool RenderPassPipeline::OnSetup(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes)
    {
        for (auto &ppl : scenePipelines) {
            ppl->Setup(rdg);
        }
        return true;
    }

} // namespace