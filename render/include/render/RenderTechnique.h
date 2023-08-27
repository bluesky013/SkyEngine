//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <rhi/Device.h>

namespace sky {

    class RenderTechnique {
    public:
        RenderTechnique() = default;
        ~RenderTechnique() = default;
    private:
        rhi::PipelineLayoutPtr pipelineLayout;
    };

    class TechniqueInstance {
        rhi::GraphicsPipelinePtr pso;
    };

} // namespace sky