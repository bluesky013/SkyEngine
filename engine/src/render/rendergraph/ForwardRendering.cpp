//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/ForwardRendering.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <engine/render/rendergraph/RenderGraphPassData.h>
#include <engine/render/DriverManager.h>
#include <vulkan/Util.h>

namespace sky {

    ForwardRendering::ForwardRendering()
    {
    }

    ForwardRendering::~ForwardRendering()
    {
    }

    void ForwardRendering::SetupImage()
    {
        auto device = DriverManager::Get()->GetDevice();

        auto& ext = swapChain->GetExtent();
        if (ext.width != extent.width || ext.height != extent.height || !depthImage) {
            auto desc = drv::MakeImage2D(VK_FORMAT_D32_SFLOAT_S8_UINT, ext);
            desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            desc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
            depthImage = device->CreateDeviceObject<drv::Image>(desc);
        }
        extent = ext;
    }

    void ForwardRendering::Render(RenderGraph& renderGraph)
    {
        SetupImage();

        renderGraph.AddPass<GraphicPassData>("ForwardColor",
            [this](RenderGraphBuilder& builder, GraphicPassData& data) -> bool {
            builder.ImportImage("MainColor", swapChain->GetImage());
            builder.ImportImage("MainDepthStencil", depthImage);
            auto color = builder.WriteImage("MainColor", drv::ImageView::Make2DColor(swapChain->GetFormat()),
                ImageBindingFlag{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE});
            color->SetClearColor(drv::MakeClearColor(1.0, 0.0, 0.0, 1.0));
            data.colors.emplace_back(color);
            data.depthStencil = builder.WriteImage("MainDepthStencil", drv::ImageView::Make2DDepthStencil(VK_FORMAT_D32_SFLOAT_S8_UINT),
                ImageBindingFlag{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE});
            data.extent2D = swapChain->GetExtent();
            BuildGraphicsPass(data);
            return true;
        }, [](GraphicPassData& data, const RenderGraph&, drv::CommandBuffer& cmdBuffer) {
            BuildFrameBuffer(data);
            GraphicPassExecutor executor(data);
            executor.Execute(cmdBuffer);
        });

        renderGraph.AddPass<GraphicPassData>("SwapChain",
        [this](RenderGraphBuilder& builder, GraphicPassData& data) -> bool {
            auto color = builder.WriteImage("MainColor", drv::ImageView::Make2DColor(swapChain->GetFormat()),
                ImageBindingFlag{ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE});
            color->SetClearColor(drv::MakeClearColor(0.0, 1.0, 0.0, 1.0));
            data.colors.emplace_back(color);
            data.extent2D = swapChain->GetExtent();
            BuildGraphicsPass(data);
            return true;
        }, [](GraphicPassData& data, const RenderGraph&, drv::CommandBuffer& cmdBuffer) {
            BuildFrameBuffer(data);
            GraphicPassExecutor executor(data);
            executor.Execute(cmdBuffer);
        });

    }

}