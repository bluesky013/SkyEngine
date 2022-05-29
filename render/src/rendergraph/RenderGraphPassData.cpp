//
// Created by Zach Lee on 2022/1/22.
//

#include <render/rendergraph/RenderGraphPassData.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <vulkan/RenderPass.h>

namespace sky {

    GraphicPassData::~GraphicPassData()
    {
        DevObjManager::Get()->FreeDeviceObject(frameBuffer);
    }

    void GraphicPassExecutor::Execute(drv::CommandBuffer& cmdBuffer)
    {
        VkRenderPassBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.renderPass = data.pass->GetNativeHandle();
        beginInfo.framebuffer = data.frameBuffer->GetNativeHandle();
        beginInfo.renderArea.extent = data.extent2D;
        beginInfo.clearValueCount = static_cast<uint32_t>(data.clears.size());
        beginInfo.pClearValues = data.clears.data();
        auto cmd = cmdBuffer.GetNativeHandle();
        vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport { 0, 0,  (float)data.extent2D.width, (float)data.extent2D.height, 0, 1.f};
        VkRect2D rect {{0, 0}, data.extent2D};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &rect);

        if (data.encoder) {
            data.encoder->Execute(cmdBuffer, data);
        }
        vkCmdEndRenderPass(cmd);
    }

    void FullscreenEncoder::Execute(drv::CommandBuffer &cmdBuffer, GraphicPassData& data)
    {
        auto cmd = cmdBuffer.GetNativeHandle();
        auto& fsData = static_cast<FullscreenPassData&>(data);
        if (fsData.pipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fsData.pipeline->GetNativeHandle());
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }
    }

    inline void BuildAttachment(drv::RenderPassFactory::AttachmentImpl builder, RGAttachmentPtr attachment)
    {
        auto subImage = attachment->GetSubImage();
        auto& lastBinding = attachment->GetLastBinding();
        auto& currentBinding = attachment->GetCurrentBinding();
        auto& attachmentDesc = attachment->GetAttachmentDesc();
        builder.Format(subImage->GetFormat())
            .Layout(lastBinding.layout, currentBinding.layout)
            .ColorOp(attachmentDesc.loadOp, attachmentDesc.storeOp)
            .StencilOp(attachmentDesc.stencilLoadOp, attachmentDesc.stencilStoreOp);
    }

    void BuildGraphicsPass(GraphicPassData& passData)
    {
        drv::RenderPassFactory factory;
        auto subPass = factory().AddSubPass();
        for (auto& color : passData.colors) {
            BuildAttachment(subPass.AddColor(), color);
        }
        if (passData.depthStencil) {
            BuildAttachment(subPass.AddDepthStencil(), passData.depthStencil);
        }
        subPass.AddDependency()
            .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
            .SetBarrier(
                drv::Barrier{VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_MEMORY_READ_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT})
            .AddDependency()
            .SetLinkage(0, VK_SUBPASS_EXTERNAL)
            .SetBarrier(
                drv::Barrier{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                    VK_ACCESS_MEMORY_READ_BIT});
        passData.pass = factory().Create(*DriverManager::Get()->GetDevice());
    }

    void BuildFrameBuffer(GraphicPassData& passData)
    {
        drv::FrameBuffer::Descriptor desc = {};
        desc.extent = passData.extent2D;
        desc.pass = passData.pass.get();

        passData.clears.clear();
        for (auto& color : passData.colors) {
            auto view = color->GetImageView();
            desc.views.emplace_back(view->GetNativeHandle());
            passData.clears.emplace_back(color->GetClearValue());
        }
        if (passData.depthStencil) {
            desc.views.emplace_back(passData.depthStencil->GetImageView()->GetNativeHandle());
            passData.clears.emplace_back(passData.depthStencil->GetClearValue());
        }
        passData.frameBuffer = DriverManager::Get()->CreateDeviceObject<drv::FrameBuffer>(desc);
    }

}