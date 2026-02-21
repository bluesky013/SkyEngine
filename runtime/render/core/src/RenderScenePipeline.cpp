//
// Created by blues on 2024/9/5.
//

#include <render/RenderScenePipeline.h>

#include "render/RenderScene.h"
#include "render/rdg/RenderGraph.h"

namespace sky {

    void RenderScenePipeline::AddPass(IPass *pass)
    {
        passes.emplace_back(pass);
    }

    void RenderScenePipeline::Setup(rdg::RenderGraph &rdg)
    {
        if (scene == nullptr) {
            return;
        }

        passes.clear();

        for (auto& [name, view] : scene->GetActiveSceneViews()) {
            rdg.AddSceneView(name, view);
        }

        Collect(rdg);

        for (auto &pass : passes) {
            pass->Prepare(rdg, *scene);
        }

        for (auto &pass : passes) {
            pass->Setup(rdg, *scene);
        }
    }

} // namespace sky