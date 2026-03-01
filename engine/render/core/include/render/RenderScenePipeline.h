//
// Created by blues on 2024/9/5.
//

#pragma once

#include <render/renderpass/PassBase.h>
#include <memory>
#include <vector>

namespace sky {

    class RenderScene;
    namespace rdg {
        struct RenderGraph;
    } // namespace rdg

    class RenderScenePipeline {
    public:
        explicit RenderScenePipeline(RenderScene *scn) : scene(scn) {}
        virtual ~RenderScenePipeline() = default;

        void AddPass(IPass *pass);

        void Setup(rdg::RenderGraph &rdg);

    protected:
        virtual void Collect(rdg::RenderGraph &rdg) {}

        RenderScene *scene = nullptr;
        std::vector<IPass*> passes;
    };

} // namespace sky
