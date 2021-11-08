//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::drv {

    class Device;

    class RenderPass : public DevObject {
    public:
        ~RenderPass();

        struct Descriptor {
            std::vector<VkAttachmentDescription> attachments;
            std::vector<VkSubpassDescription> subpass;
            std::vector<VkSubpassDependency> dependencies;
        };

        bool Init(const Descriptor&);

        VkRenderPass GetNativeHandle() const;

    private:
        friend class Device;
        RenderPass(Device&);

        VkRenderPass pass;
    };

}
