//
// Created by blues on 2024/9/3.
//

#pragma once

#include <render/RenderPipeline.h>
#include <render/RenderScenePipeline.h>
#include <vector>
#include <memory>

namespace sky {

    class RenderPassPipeline : public RenderPipeline {
    public:
        RenderPassPipeline() = default;
        ~RenderPassPipeline() override = default;

        void AddScenePass(RenderScenePipeline *ppl);
    private:
        bool OnSetup(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes) override;

        std::vector<std::unique_ptr<RenderScenePipeline>> scenePipelines;
    };

} // namespace sky
