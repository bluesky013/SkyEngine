//
// Created by Zach Lee on 2023/8/27.
//

#pragma once

#include <rhi/Device.h>

namespace sky {

    class RasterTechnique {
    public:
        RasterTechnique() = default;
        ~RasterTechnique() = default;
    private:
        rhi::PipelineLayoutPtr pipelineLayout;
        rhi::PipelineState state;
    };

} // namespace sky