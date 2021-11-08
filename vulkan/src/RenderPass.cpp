//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/RenderPass.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "core/logger/Logger.h"
static const char* TAG = "Driver";

namespace sky::drv {

    RenderPass::RenderPass(Device& dev) : DevObject(dev), pass(VK_NULL_HANDLE)
    {
    }

    RenderPass::~RenderPass()
    {
        if (pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device.GetNativeHandle(), pass, VKL_ALLOC);
        }
    }

    bool RenderPass::Init(const Descriptor& des)
    {
        VkRenderPassCreateInfo passInfo = {};
        passInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        passInfo.attachmentCount = (uint32_t)des.attachments.size();
        passInfo.pAttachments    = des.attachments.data();
        passInfo.subpassCount    = (uint32_t)des.subpass.size();
        passInfo.pSubpasses      = des.subpass.data();
        passInfo.dependencyCount = (uint32_t)des.dependencies.size();
        passInfo.pDependencies   = des.dependencies.data();

        auto rst = vkCreateRenderPass(device.GetNativeHandle(), &passInfo, VKL_ALLOC, &pass);
        if (rst != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    VkRenderPass RenderPass::GetNativeHandle() const
    {
        return pass;
    }
}