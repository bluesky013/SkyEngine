//
// Created by Zach Lee on 2022/8/20.
//

#pragma once

#include <render/RenderPrimitive.h>
#include <render/RenderConstants.h>
#include <render/resources/DescirptorGroup.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky {

    class GuiPrimitive : public RenderPrimitive {
    public:
        GuiPrimitive() = default;
        ~GuiPrimitive() = default;

        void Encode(RenderRasterEncoder*) override;

        void SetDrawTag(uint32_t tag);

    private:
        friend class GuiRenderer;
        RDDesGroupPtr set;
        drv::GraphicsPipelinePtr pso;
        drv::DescriptorSetBinderPtr setBinder;
        drv::VertexAssemblyPtr assembly;
        uint32_t drawTag;
    };

}