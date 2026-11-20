//
// Created by blues on 2024/12/8.
//

#include <render/adaptor/pipeline/DecalPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/rdg/RenderGraph.h>
#include <render/decal/DecalFeatureProcessor.h>

namespace sky {

    DecalPass::DecalPass(const RDGfxTechPtr &tech, const Name &colorName, const Name &depthName)
        : FullScreenPass(Name("DecalPass"), tech)
    {
        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS;

        // Output: write into the opaque colour target (LoadOp::LOAD keeps existing pixels)
        colors.emplace_back(Attachment{
            rdg::RasterAttachment{colorName, rhi::LoadOp::LOAD, rhi::StoreOp::STORE},
            rhi::ClearValue(0.f, 0.f, 0.f, 0.f)
        });

        // Input: decal UBO (count + per-decal OBB + colour)
        computeResources.emplace_back(ComputeResource{
            Name(DECAL_PASS_INFO.data()),
            rdg::ComputeView{Name("DecalPassInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        // Input: scene view UBO (for InvViewProj)
        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        // Input: scene depth texture (to reconstruct world position)
        computeResources.emplace_back(ComputeResource{
            depthName,
            rdg::ComputeView{Name("DepthBuffer"), rdg::ComputeType::SRV, stageFlags}
        });

        // Sampler for depth texture (reuse the global PointSampler imported by the pipeline)
        samplers.emplace_back(SamplerResource{Name("PointSampler"), Name("DepthSampler")});

        // Descriptor set layout for this pass
        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, stageFlags, "DecalPassInfo");
        desc.bindings.emplace_back(rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, stageFlags, "viewInfo");
        desc.bindings.emplace_back(rhi::DescriptorType::SAMPLED_IMAGE,  1, 2, stageFlags, "DepthBuffer");
        desc.bindings.emplace_back(rhi::DescriptorType::SAMPLER,        1, 3, stageFlags, "DepthSampler");

        decalLayout = new ResourceGroupLayout();
        decalLayout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        decalLayout->AddNameHandler(Name("DecalPassInfo"), {0, sizeof(DecalPassInfo)});
        decalLayout->AddNameHandler(Name("viewInfo"),      {1, sizeof(SceneViewInfo)});
        decalLayout->AddNameHandler(Name("DepthBuffer"),   {2});
        decalLayout->AddNameHandler(Name("DepthSampler"),  {3});
    }

    void DecalPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        // Skip the pass when no decals are active
        auto *decalFP = scene.GetFeature<DecalFeatureProcessor>();
        if (decalFP == nullptr) {
            return;
        }

        FullScreenPass::Setup(rdg, scene);
    }

} // namespace sky
