//
// Created by blues on 2024/9/5.
//

#include <render/RenderScenePipeline.h>

namespace sky {

    void RenderScenePipeline::AddPass(PassBase *pass)
    {
        passes.emplace_back(pass);
    }

    void RenderScenePipeline::Setup(rdg::RenderGraph &rdg)
    {
        if (scene == nullptr) {
            return;
        }

        passes.clear();
        Collect(rdg);

        for (auto &pass : passes) {
            pass->Prepare(rdg, *scene);
        }

        for (auto &pass : passes) {
            pass->Setup(rdg, *scene);
        }
    }

} // namespace sky