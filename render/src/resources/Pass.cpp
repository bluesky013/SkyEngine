//
// Created by Zach Lee on 2022/7/30.
//

#include <render/DriverManager.h>
#include <render/resources/Pass.h>

namespace sky {

    void Pass::InitRHI()
    {
        if (renderPass) {
            return;
        }

        using AF = drv::RenderPassFactory::AttachmentImpl;
        using DF = drv::RenderPassFactory::DependencyImpl;
        using PF = drv::RenderPassFactory::Impl;

        drv::RenderPassFactory factory;
        PF                     pF = factory();

        auto attachmentFn = [](AF af, VkFormat format, VkSampleCountFlagBits samples, VkImageLayout layout) {
            af.ColorOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE)
                .StencilOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE)
                .Layout(VK_IMAGE_LAYOUT_UNDEFINED, layout)
                .Format(format)
                .Samples(samples);
        };

        auto dependencyFn = [](DF df, const PassDependencyInfo &info) {
            df.SetLinkage(info.src, info.dst).SetBarrier(info.barrier).SetFlags(VK_DEPENDENCY_BY_REGION_BIT);
        };

        for (auto &subPass : subPasses) {
            auto subFactory = pF.AddSubPass();
            for (auto &color : subPass.colors) {
                attachmentFn(subFactory.AddColor(), color.format, color.samples, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }

            for (auto &color : subPass.colors) {
                if (color.samples > VK_SAMPLE_COUNT_1_BIT) {
                    attachmentFn(subFactory.AddResolve(), color.format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                }
            }

            for (auto &input : subPass.inputs) {
                attachmentFn(subFactory.AddInput(), input.format, input.samples, input.usage);
            }

            if (subPass.depthStencil.format != VK_FORMAT_UNDEFINED) {
                attachmentFn(subFactory.AddDepthStencil(), subPass.depthStencil.format, subPass.depthStencil.samples,
                             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
        }

        dependencyFn(pF.AddDependency(), {VK_SUBPASS_EXTERNAL,
                                          0,
                                          {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT}});
        for (auto &dep : dependencies) {
            dependencyFn(pF.AddDependency(), dep);
        }
        dependencyFn(pF.AddDependency(), {0,
                                          VK_SUBPASS_EXTERNAL,
                                          {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT}});

        auto device = DriverManager::Get()->GetDevice();
        renderPass  = pF.Create(*device);
    }

    bool Pass::IsValid() const
    {
        return !!renderPass;
    }

    void Pass::AddSubPass(const SubPassInfo &subPassInfo)
    {
        subPasses.emplace_back(subPassInfo);
    }

    drv::RenderPassPtr Pass::GetRenderPass() const
    {
        return renderPass;
    }

    void Pass::ValidatePipelineState(drv::GraphicsPipeline::State &state, uint32_t index)
    {
        if (index >= subPasses.size()) {
            return;
        }

        auto &subPass              = subPasses[index];
        state.blends.attachmentNum = static_cast<uint32_t>(subPass.colors.size());
        state.multiSample.samples  = subPass.colors.empty() ? subPass.depthStencil.samples : subPass.colors[0].samples;
    }

} // namespace sky