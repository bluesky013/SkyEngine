//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <rhi/Device.h>
#include <render/resource/Material.h>
#include <render/resource/Technique.h>
#include <render/RenderPackage.h>

namespace sky {

    struct TechniqueInstance {
        uint32_t viewMask = 0xFFFFFFFF;  // mask all
        uint32_t rasterID = ~(0U);       // invalid id

        rhi::GraphicsPipelinePtr pso;
    };

    struct RenderPrimitive {
        std::vector<TechniqueInstance> techniques;

        uint32_t sortKey = 0;

        rhi::DescriptorSetPtr batchSet;
        rhi::DescriptorSetPtr instanceSet;

        rhi::VertexAssemblyPtr va;
        rhi::BufferPtr indirectBuffer;
        DrawArgs args;
    };

    struct RenderDrawItem {
        RenderPrimitive *primitive = nullptr;
        uint32_t techIndex = 0;
    };

} // namespace sky
