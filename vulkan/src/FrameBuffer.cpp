//
// Created by Zach Lee on 2021/12/13.
//

#include <vulkan/Device.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/RenderPass.h>

namespace sky::vk {

    FrameBuffer::FrameBuffer(Device &dev) : DevObject(dev), frameBuffer(VK_NULL_HANDLE)
    {
    }

    FrameBuffer::~FrameBuffer()
    {
        if (frameBuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device.GetNativeHandle(), frameBuffer, VKL_ALLOC);
        }
    }

    bool FrameBuffer::Init(const Descriptor &des)
    {
        descriptor.extent = {des.extent.width, des.extent.height};
        descriptor.pass = std::static_pointer_cast<RenderPass>(des.pass);

        std::vector<VkImageView> views;
        for (auto &view : des.views) {
            descriptor.views.emplace_back(std::static_pointer_cast<ImageView>(view));
            views.emplace_back(descriptor.views.back()->GetNativeHandle());
        }

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass              = descriptor.pass->GetNativeHandle();
        fbInfo.width                   = des.extent.width;
        fbInfo.height                  = des.extent.height;
        fbInfo.layers                  = 1;
        fbInfo.attachmentCount         = (uint32_t)des.views.size();
        fbInfo.pAttachments            = views.data();

        auto rst = vkCreateFramebuffer(device.GetNativeHandle(), &fbInfo, VKL_ALLOC, &frameBuffer);
        if (rst != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    bool FrameBuffer::Init(const VkDescriptor &des)
    {
        std::vector<VkImageView> views(des.views.size());
        for (uint32_t i = 0; i < des.views.size(); ++i) {
            views[i] = des.views[i]->GetNativeHandle();
        }

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass              = des.pass->GetNativeHandle();
        fbInfo.width                   = des.extent.width;
        fbInfo.height                  = des.extent.height;
        fbInfo.layers                  = 1;
        fbInfo.attachmentCount         = (uint32_t)des.views.size();
        fbInfo.pAttachments            = views.data();

        auto rst = vkCreateFramebuffer(device.GetNativeHandle(), &fbInfo, VKL_ALLOC, &frameBuffer);
        if (rst != VK_SUCCESS) {
            return false;
        }

        descriptor = des;
        return true;
    }

    VkFramebuffer FrameBuffer::GetNativeHandle() const
    {
        return frameBuffer;
    }

    const VkExtent2D &FrameBuffer::GetExtent() const
    {
        return descriptor.extent;
    }

    uint32_t FrameBuffer::GetAttachmentCount() const
    {
        return static_cast<uint32_t>(descriptor.views.size());
    }
} // namespace sky::vk
