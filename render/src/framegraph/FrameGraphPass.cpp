//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphPass.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>

namespace sky {

    VkImageLayout GetImageLayout(ImageBindFlag flag)
    {
        switch (flag) {
            case ImageBindFlag::UNDEFINED:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageBindFlag::COLOR:
            case ImageBindFlag::COLOR_RESOLVE:
            case ImageBindFlag::INPUT:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ImageBindFlag::DEPTH_STENCIL:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageBindFlag::DEPTH_STENCIL_READ:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case ImageBindFlag::SHADER_READ:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ImageBindFlag::PRESENT:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:
                break;
        }
        return VK_IMAGE_LAYOUT_GENERAL;
    }

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
            default:
                break;
        }
    }

    FrameGraphGraphicPass::~FrameGraphGraphicPass()
    {
        DevObjManager::Get()->FreeDeviceObject(passInfo.renderPass);
        DevObjManager::Get()->FreeDeviceObject(passInfo.frameBuffer);
    }

    void FrameGraphGraphicPass::Compile()
    {
        drv::FrameBuffer::Descriptor fbDesc = {};

        using AF = drv::RenderPassFactory::AttachmentImpl;
        drv::RenderPassFactory factory;
        auto subPassFactory = factory().AddSubPass();
        auto attachmentFn =
            [&fbDesc](AF af, FrameGraphImageAttachment* attachment)
            {
                attachment->Compile();
                auto& imageInfo = attachment->source->GetImageInfo();
                fbDesc.extent.width = imageInfo.extent.width;
                fbDesc.extent.height = imageInfo.extent.height;

                af.ColorOp(attachment->loadOp, attachment->storeOp)
                    .StencilOp(attachment->stencilLoadOp, attachment->stencilStoreOp)
                    .Layout(GetImageLayout(attachment->initFlag), GetImageLayout(attachment->finalFlag))
                    .Format(imageInfo.format)
                    .Samples(imageInfo.samples);

                fbDesc.views.emplace_back(attachment->imageView);
            };

        for (auto& color : colors) {
            attachmentFn(subPassFactory.AddColor(), color);
        }

        for (auto& resolve : resolves) {
            attachmentFn(subPassFactory.AddResolve(), resolve);
        }

        if (depthStencil != nullptr) {
            attachmentFn(subPassFactory.AddDepthStencil(), depthStencil);
        }

        subPassFactory.AddDependency()
            .SetLinkage(VK_SUBPASS_EXTERNAL, 0)
            .SetBarrier({VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT})
            .AddDependency()
            .SetLinkage(0, VK_SUBPASS_EXTERNAL)
            .SetBarrier({VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT});

        auto device = DriverManager::Get()->GetDevice();
        fbDesc.pass = passInfo.renderPass = subPassFactory.Create(*device);
        passInfo.frameBuffer = device->CreateDeviceObject<drv::FrameBuffer>(fbDesc);
        passInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        passInfo.clearValues = clearValues.data();
        passInfo.contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
    }

    void FrameGraphGraphicPass::Execute(const drv::CommandBufferPtr &commandBuffer)
    {
        auto drvEncoder = commandBuffer->EncodeGraphics();
        drvEncoder.BeginPass(passInfo);

        if (encoder) {
            encoder->Encode(drvEncoder);
        }
        drvEncoder.EndPass();
    }

    void FrameGraphGraphicPass::SetEncoder(RenderEncoder* value)
    {
        encoder = value;
    }

    drv::RenderPassPtr FrameGraphGraphicPass::GetPass() const
    {
        return passInfo.renderPass;
    }
}