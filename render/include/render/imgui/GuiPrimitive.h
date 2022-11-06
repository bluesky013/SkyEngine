//
// Created by Zach Lee on 2022/8/20.
//

#pragma once

#include <render/RenderConstants.h>
#include <render/RenderPrimitive.h>
#include <render/resources/DescirptorGroup.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/DrawItem.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/PushConstants.h>
#include <vulkan/VertexAssembly.h>

namespace sky {

    class GuiPrimitive : public RenderPrimitive {
    public:
        GuiPrimitive()  = default;
        ~GuiPrimitive() = default;

        void Encode(RenderRasterEncoder *) override;

        void SetDrawTag(uint32_t tag);

    private:
        friend class GuiRenderer;
        RDDesGroupPtr               set;
        vk::GraphicsPipelinePtr    pso;
        vk::DescriptorSetBinderPtr setBinder;
        vk::VertexAssemblyPtr      assembly;
        vk::PushConstantsPtr       constants;
        uint32_t                    drawTag;
        struct DrawCall {
            VkRect2D            scissor;
            vk::CmdDrawIndexed indexed;
        };

        std::vector<DrawCall> dc;
    };

} // namespace sky
