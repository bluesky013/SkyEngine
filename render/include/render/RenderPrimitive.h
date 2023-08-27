//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <rhi/Device.h>
#include <render/MaterialProxy.h>
#include <render/RenderTechnique.h>
#include <render/RenderPackage.h>

namespace sky {

    struct TechniqueInstance {
        uint32_t viewMask = 0xFFFFFFFF;  // mask all
        uint32_t rasterID = ~(0U);       // invalid id
        rhi::GraphicsPipelinePtr pso;
    };

    struct RenderPrimitive {
        MaterialProxyPtr material;
        std::vector<TechniqueInstance> techniques;

        uint32_t sortKey = 0;

        rhi::VertexAssemblyPtr va;
        rhi::DescriptorSetPtr set;
        rhi::DescriptorSetLayoutPtr localLayout;
        rhi::BufferPtr indirectBuffer;

        DrawArgs args;
    };

    struct RenderDrawItem {
        RenderPrimitive *primitive = nullptr;
        uint32_t techIndex = 0;
    };

} // namespace sky
