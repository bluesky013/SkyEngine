//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphPass.h>
#include <render/DriverManager.h>

namespace sky {

    void FrameGraphGraphicPass::AddClearValue(FrameGraphImageAttachment* attachment)
    {
        clearValues.emplace_back(attachment->clearValue);
    }

    void FrameGraphGraphicPass::UseImageAttachment(FrameGraphImageAttachment* attachment)
    {
        const auto& flag = attachment->bindFlag;
        switch (flag) {
            case ImageBindFlag::COLOR:
                colors.emplace_back(attachment);
                AddClearValue(attachment);
                break;
            case ImageBindFlag::COLOR_RESOLVE:
                resolves.emplace_back(attachment);
                AddClearValue(attachment);
                break;
            case ImageBindFlag::INPUT:
                inputs.emplace_back(attachment);
                AddClearValue(attachment);
                break;
            case ImageBindFlag::DEPTH_STENCIL:
                depthStencil = attachment;
                AddClearValue(attachment);
                break;
        }
    }

    void FrameGraphGraphicPass::Compile()
    {
        drv::FrameBuffer::Descriptor fbDesc = {};

        using AF = drv::RenderPassFactory::AttachmentImpl;
        drv::RenderPassFactory factory;
        auto subPassFactory = factory().AddSubPass();
        auto attachmentFn =
            [&fbDesc](AF af, FrameGraphImageAttachment* attachment, VkImageLayout layout)
            {
                attachment->Compile();
                auto& imageInfo = attachment->source->GetImageInfo();
                fbDesc.extent.width = imageInfo.extent.width;
                fbDesc.extent.height = imageInfo.extent.height;

                af.ColorOp(attachment->loadOp, attachment->storeOp)
                    .StencilOp(attachment->stencilLoadOp, attachment->stencilStoreOp)
                    .Layout(layout, layout)
                    .Format(imageInfo.format)
                    .Samples(imageInfo.samples);

                fbDesc.views.emplace_back(attachment->imageView);
            };

        for (auto& color : colors) {
            attachmentFn(subPassFactory.AddColor(), color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        for (auto& resolve : resolves) {
            attachmentFn(subPassFactory.AddResolve(), resolve, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        if (depthStencil != nullptr) {
            attachmentFn(subPassFactory.AddDepthStencil(), depthStencil, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        auto device = DriverManager::Get()->GetDevice();
        fbDesc.pass = passInfo.renderPass = subPassFactory.Create(*device);
        passInfo.frameBuffer = device->CreateDeviceObject<drv::FrameBuffer>(fbDesc);
        passInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passInfo.clearValues = clearValues.data();
    }

    void FrameGraphGraphicPass::Execute(drv::CommandBufferPtr commandBuffer)
    {
        auto encoder = commandBuffer->EncodeGraphics();
        encoder.BeginPass(passInfo);
        encoder.EndPass();
    }

}