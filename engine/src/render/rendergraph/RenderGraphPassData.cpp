//
// Created by Zach Lee on 2022/1/22.
//

#include <engine/render/rendergraph/RenderGraphPassData.h>
#include <engine/render/DriverManager.h>
#include <vulkan/RenderPass.h>

namespace sky {

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