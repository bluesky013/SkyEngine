//
// Created by blues on 2024/9/5.
//

#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PostProcessingPass::PostProcessingPass(const RDGfxTechPtr &tech)
        : FullScreenPass(Name("PostProcessingPass"), tech)
    {
        Name swapChainName(SWAP_CHAIN.data());
        Name fwdColor(FWD_CL.data());

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{swapChainName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            fwdColor,
            rdg::ComputeView{Name("InColor"), rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS}
        });

        rhi::DescriptorSetLayout::Descriptor desc = {};
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::FS, "passInfo");
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
                rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo"});
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::SAMPLED_IMAGE, 1, 2, rhi::ShaderStageFlagBit::FS, "ShadowMap");
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::SAMPLER, 1, 3, rhi::ShaderStageFlagBit::FS, "ShadowMapSampler");

        debugLayout = new ResourceGroupLayout();
        debugLayout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        debugLayout->AddNameHandler(Name("passInfo"), {0, sizeof(ShaderPassInfo)});
        debugLayout->AddNameHandler(Name("viewInfo"), {1, sizeof(SceneViewInfo)});
//        layout->AddNameHandler("ShadowMap", {2});
//        layout->AddNameHandler("ShadowMapSampler", {3});
    }

    void PostProcessingPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        FullScreenPass::Setup(rdg, scene);
    }

    void PostProcessingPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        builder.AddQueue(Name("debug")).SetRasterID(Name("debug")).SetLayout(debugLayout);
        builder.AddQueue(Name("queue")).SetRasterID(Name("ui"));
    }

} // namespace sky
