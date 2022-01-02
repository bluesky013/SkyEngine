//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/ForwardRendering.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <engine/render/rendergraph/RenderGraphPassData.h>

namespace sky {

    static constexpr const char* DirectShadowMap  = "DirectShadowMap";
    static constexpr const char* MainDepthStencil = "MainDepthStencil";
    static constexpr const char* GBufferNormal    = "GBufferNormal";
    static constexpr const char* GBufferAlbedo    = "GBufferAlbedo";

    ForwardRendering::ForwardRendering()
    {
    }

    ForwardRendering::~ForwardRendering()
    {
    }

    void ForwardRendering::Render(RenderGraph& renderGraph)
    {
        if (!swapChain) {
            return;
        }

        renderGraph.AddPass<GraphicPassData>("Forward",[this](RenderGraphBuilder& builder, GraphicPassData& data) {
            drv::Image::Descriptor desc = {};
            auto& ext = swapChain->GetExtent();
            desc.extent = VkExtent3D{ext.width, ext.height, 1};
            desc.format = VK_FORMAT_D24_UNORM_S8_UINT;
            desc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
            desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            desc.transient = true;
            builder.CreateImage("MainDepthStencilImage", desc);

            desc.format = swapChain->GetFormat();
            builder.CreateImage("MainColorImage", desc);

            drv::ImageView::Descriptor viewDesc = {};
            viewDesc.format = swapChain->GetFormat();
            viewDesc.subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            builder.CreateAttachment("MainColorImage", "MainColor", viewDesc);

            viewDesc.format = VK_FORMAT_D24_UNORM_S8_UINT;
            viewDesc.subResourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            builder.CreateAttachment("MainDepthStencilImage", "MainDepthStencil", viewDesc);

            builder.Write("MainColor");
            builder.Write("MainDepthStencil");
        }, [](const GraphicPassData& data, const RenderGraph&, drv::CommandBuffer& cmd) {
            cmd.Encode([](VkCommandBuffer commandBuffer) {
                VkClearValue clear;
                clear.color.float32[0] = 1.f;
                clear.color.float32[1] = 0.f;
                clear.color.float32[2] = 0.f;
                clear.color.float32[3] = 1.f;

                VkRenderPassBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                beginInfo.pNext = nullptr;
//                beginInfo.renderPass = pass->GetNativeHandle();
//                beginInfo.framebuffer = fb->GetNativeHandle();
//                beginInfo.renderArea = {{0, 0}, ext};
                beginInfo.clearValueCount = 1;
                beginInfo.pClearValues = &clear;

                vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdEndRenderPass(commandBuffer);
            });
        });

    }

}