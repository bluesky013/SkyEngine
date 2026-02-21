//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include "rhi/RenderPass.h"
#include "vulkan/Basic.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::vk {

    class Device;

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        ~RenderPass() override = default;

        VkRenderPass GetNativeHandle() const { return pass; }
        uint32_t GetHash() const { return hash; }
        const std::vector<VkAttachmentDescription2> &GetAttachments() const { return attachments; }

    private:
        friend class Device;
        explicit RenderPass(Device &);

        bool Init(const Descriptor &);

        VkRenderPass pass;
        uint32_t hash = 0;
        std::vector<VkAttachmentDescription2>  attachments;
        std::vector<VkSubpassDescription2>     subPasses;
        std::vector<VkSubpassDependency2>      dependencies;
        VkFragmentShadingRateAttachmentInfoKHR shadingRateInfo = {VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR};
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;
} // namespace sky::vk
