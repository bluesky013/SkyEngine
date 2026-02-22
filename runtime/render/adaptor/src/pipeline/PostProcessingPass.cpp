//
// Created by blues on 2024/9/5.
//

#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PostProcessingPass::PostProcessingPass(const RDGfxTechPtr &tech)
        : FullScreenPass(Name("PostProcessingPass"), tech)
    {
        Name swapChainName(SWAP_CHAIN.data());
        Name fwdColor(FWD_CL.data());

        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        colors.emplace_back(Attachment{
            rdg::RasterAttachment{swapChainName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            fwdColor,
            rdg::ComputeView{Name("InColor"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, stageFlags, "passInfo");
        desc.bindings.emplace_back(rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, stageFlags, "viewInfo");

        debugLayout = new ResourceGroupLayout();
        debugLayout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        debugLayout->AddNameHandler(Name("passInfo"), {0, sizeof(ShaderPassInfo)});
        debugLayout->AddNameHandler(Name("viewInfo"), {1, sizeof(SceneViewInfo)});
    }

    void PostProcessingPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        FullScreenPass::Setup(rdg, scene);
    }

    void PostProcessingPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        static const Name ViewName("MainCamera");
        static const Name DebugQueueName("debug");
        static const Name DebugRasterId("debug");
        static const Name UIQueueName("UI");
        static const Name UIRasterId("ui");

        builder.AddQueue(DebugQueueName)
            .SetRasterID(DebugRasterId)
            .SetSceneView(ViewName)
            .SetLayout(debugLayout);
        builder.AddQueue(UIQueueName).SetRasterID(UIRasterId);
    }

} // namespace sky
