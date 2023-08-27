//
// Created by Zach Lee on 2023/8/20.
//

#pragma once

#include <render/rdg/RenderGraphContext.h>

namespace sky {
    namespace rdg {
        struct RenderGraph;
    }

    class RenderPipeline {
    public:
        RenderPipeline();
        virtual ~RenderPipeline() = default;

        virtual void OnSetup(rdg::RenderGraph &rdg) = 0;

        void FrameSync();
        void Execute(rdg::RenderGraph &rdg);

        rdg::RenderGraphContext *Context() const { return rdgContext.get(); }

    private:
        std::unique_ptr<rdg::RenderGraphContext> rdgContext;
    };

} // namespace sky