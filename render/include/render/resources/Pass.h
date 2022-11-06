//
// Created by Zach Lee on 2022/7/30.
//

#pragma once

#include <memory>
#include <render/resources/RenderResource.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/RenderPass.h>

namespace sky {

    struct AttachmentInfo {
        VkFormat              format  = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    struct InputAttachmentInfo {
        VkFormat              format  = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    struct SubPassInfo {
        std::vector<AttachmentInfo>      colors;
        std::vector<InputAttachmentInfo> inputs;
        AttachmentInfo                   depthStencil;
    };

    struct PassDependencyInfo {
        uint32_t     src = 0;
        uint32_t     dst = 0;
    };

    class Pass : public RenderResource {
    public:
        Pass()  = default;
        ~Pass() = default;

        void InitRHI();

        bool IsValid() const override;

        void AddSubPass(const SubPassInfo &subPassInfo);

        vk::RenderPassPtr GetRenderPass() const;

        void ValidatePipelineState(vk::GraphicsPipeline::State &state, uint32_t subPass);

    private:
        std::vector<SubPassInfo>        subPasses;
        std::vector<PassDependencyInfo> dependencies;
        vk::RenderPassPtr              renderPass;
    };
    using RDPassPtr = std::shared_ptr<Pass>;
} // namespace sky
