//
// Created by Zach Lee on 2023/8/19.
//

#pragma once

#include <rhi/Device.h>
#include <render/MaterialProxy.h>
#include <render/RenderTechnique.h>

namespace sky {

    class RenderPrimitive {
    public:
        RenderPrimitive() = default;
        ~RenderPrimitive() = default;
    private:
        MaterialProxyPtr material;

        rhi::VertexAssemblyPtr va;
        rhi::DescriptorSetPtr set;
        rhi::DescriptorSetLayoutPtr localLayout;
        rhi::BufferPtr indirectBuffer;
    };

} // namespace sky
