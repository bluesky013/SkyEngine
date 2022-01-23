//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/ForwardRendering.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <engine/render/rendergraph/RenderGraphPassData.h>
#include <engine/render/DriverManager.h>
#include <vulkan/Util.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    ForwardRendering::ForwardRendering()
    {
    }

    ForwardRendering::~ForwardRendering()
    {
    }

    void ForwardRendering::SetupShader()
    {
        auto device = DriverManager::Get();
        if (!shader) {
            auto asset = AssetManager::Get()->LoadAsset("../shaders/Fullscreen.prog", ShaderAsset::TYPE);
            shader = Shader::CreateFromAsset(asset);
        }

        if (!vInput) {
            drv::VertexInput::Builder builder;
            vInput = builder.Begin().Build();
        }

        if (!pipelineLayout) {
            drv::PipelineLayout::Descriptor desc = {};
//            drv::DescriptorSetLayout::Descriptor desDesc = {};
//            desDesc.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding {
//                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                1, VK_SHADER_STAGE_FRAGMENT_BIT, 0
//            });
            pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(desc);
        }
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

            desc = drv::MakeImage2D(swapChain->GetFormat(), ext);
            desc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            desc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
            colorImage = device->CreateDeviceObject<drv::Image>(desc);
        }
        extent = ext;
    }

    void ForwardRendering::Render(RenderGraph& renderGraph)
    {
        SetupImage();
        SetupShader();

        renderGraph.AddPass<GraphicPassData>("ForwardColor",
            [this](RenderGraphBuilder& builder, GraphicPassData& data) -> bool {
            builder.ImportImage("MainColor", colorImage);
            builder.ImportImage("MainDepthStencil", depthImage);
            auto color = builder.WriteImage("MainColor", drv::ImageView::Make2DColor(swapChain->GetFormat()),
                ImageBindingFlag{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE});
            color->SetClearColor(drv::MakeClearColor(1.0, 0.0, 0.0, 1.0));
            data.colors.emplace_back(color);
            data.depthStencil = builder.WriteImage("MainDepthStencil", drv::ImageView::Make2DDepthStencil(VK_FORMAT_D32_SFLOAT_S8_UINT),
                ImageBindingFlag{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE});
            data.extent2D = extent;
            BuildGraphicsPass(data);
            return true;
        }, [](GraphicPassData& data, const RenderGraph&, drv::CommandBuffer& cmdBuffer) {
            BuildFrameBuffer(data);
            GraphicPassExecutor executor(data);
            executor.Execute(cmdBuffer);
        });

        renderGraph.AddPass<FullscreenPassData>("SwapChain",
        [this](RenderGraphBuilder& builder, FullscreenPassData& data) -> bool {
            builder.ImportImage("OutColor", swapChain->GetImage());
            auto color = builder.WriteImage("OutColor", drv::ImageView::Make2DColor(swapChain->GetFormat()),
                ImageBindingFlag{ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                AttachmentDesc{VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE});
            color->SetClearColor(drv::MakeClearColor(0.0, 0.0, 0.0, 0.0));
            data.colors.emplace_back(color);
            data.extent2D = extent;
            BuildGraphicsPass(data);

            drv::GraphicsPipeline::Descriptor desc = {};
            desc.state = &shader->GetState();
            desc.program = &shader->GetProgram();
            desc.pipelineLayout = pipelineLayout;
            desc.vertexInput = vInput;
            desc.renderPass = data.pass;
            data.pipeline = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::GraphicsPipeline>(desc);

            return true;
        }, [](FullscreenPassData& data, const RenderGraph&, drv::CommandBuffer& cmdBuffer) {
            BuildFrameBuffer(data);
            GraphicPassExecutor executor(data);
            executor.Execute(cmdBuffer);
        });

    }

}