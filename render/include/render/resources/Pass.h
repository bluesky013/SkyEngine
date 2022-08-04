//
// Created by Zach Lee on 2022/7/30.
//

#pragma once

#include <render/resources/RenderResource.h>
#include <vulkan/RenderPass.h>
#include <vulkan/GraphicsPipeline.h>
#include <memory>

namespace sky {

    struct AttachmentInfo {
        VkFormat              format  = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    struct InputAttachmentInfo {
        VkFormat              format  = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout         usage   = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct SubPassInfo {
        std::vector<AttachmentInfo> colors;
        std::vector<InputAttachmentInfo> inputs;
        AttachmentInfo depthStencil;
    };

    struct PassDependencyInfo {
        uint32_t src = 0;
        uint32_t dst = 0;
        drv::Barrier barrier{};
    };

    class Pass : public RenderResource {
    public:
        Pass() = default;
        ~Pass() = default;

        void InitRHI();

        bool IsValid() const override;

        void AddSubPass(const SubPassInfo& subPassInfo);

        drv::RenderPassPtr GetRenderPass() const;

        void ValidatePipelineState(drv::GraphicsPipeline::State& state, uint32_t subPass);

    private:
        std::vector<SubPassInfo> subPasses;
        std::vector<PassDependencyInfo> dependencies;
        drv::RenderPassPtr renderPass;
    };
    using RDPassPtr = std::shared_ptr<Pass>;
}
