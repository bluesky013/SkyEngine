//
// Created by Zach Lee on 2021/12/13.
//

#include <vulkan/FrameBuffer.h>
#include <vulkan/RenderPass.h>
#include <vulkan/Device.h>

namespace sky::drv {

    FrameBuffer::FrameBuffer(Device& dev)
        : DevObject(dev)
        , frameBuffer(VK_NULL_HANDLE)
    {

    }

    FrameBuffer::~FrameBuffer()
    {
        if (frameBuffer != nullptr) {
            vkDestroyFramebuffer(device.GetNativeHandle(), frameBuffer, VKL_ALLOC);
        }
    }

    bool FrameBuffer::Init(const Descriptor& des)
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = des.pass->GetNativeHandle();
        fbInfo.width      = des.width;
        fbInfo.height     = des.height;
        fbInfo.layers     = 1;
        fbInfo.attachmentCount = (uint32_t)des.views.size();
        fbInfo.pAttachments = des.views.data();

        auto rst = vkCreateFramebuffer(device.GetNativeHandle(), &fbInfo, VKL_ALLOC, &frameBuffer);
        if (rst != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    VkFramebuffer FrameBuffer::GetNativeHandle() const
    {
        return frameBuffer;
    }
}