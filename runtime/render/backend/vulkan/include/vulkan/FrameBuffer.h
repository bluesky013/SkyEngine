//
// Created by Zach Lee on 2021/12/13.
//

#pragma once

#include "rhi/FrameBuffer.h"
#include "vulkan/Basic.h"
#include "vulkan/DevObject.h"
#include "vulkan/ImageView.h"
#include "vulkan/RenderPass.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::vk {

    class Device;

    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        ~FrameBuffer() override;

        struct VkDescriptor {
            VkExtent2D                extent = {1, 1};
            RenderPassPtr             pass;
            std::vector<ImageViewPtr> views;
        };

        VkFramebuffer GetNativeHandle() const { return frameBuffer; }
        const VkExtent2D &GetVkExtent() const { return descriptor.extent; }

    private:
        friend class Device;
        explicit FrameBuffer(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkFramebuffer frameBuffer;
        VkDescriptor  descriptor;
    };

    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

} // namespace sky::vk
