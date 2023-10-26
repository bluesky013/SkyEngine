//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/PipelineLayout.h>
#include <rhi/Shader.h>

namespace sky::rhi {

    class ComputePipeline {
    public:
        ComputePipeline()          = default;
        virtual ~ComputePipeline() = default;

        struct Descriptor {
            ShaderPtr         shader;
            PipelineLayoutPtr pipelineLayout;
        };
    };

}