//
// Created by blues on 2025/2/3.
//

#include <render/adaptor/pipeline/DepthPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/RenderScenePipeline.h>
#include <rhi/Util.h>

namespace sky {

    DepthPass::DepthPass(rhi::PixelFormat ds, rhi::SampleCount samples_)
        : RasterPass(Name("DepthPass"))
    {
        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.samples       = samples_;
        image.usage         = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format        = ds;

        Name fwdDepthResolve(FWD_DS_RESOLVE.data());

        images.emplace_back(fwdDepthResolve, image);

        depthStencil = Attachment{
                    rdg::RasterAttachment{fwdDepthResolve, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE},
            rhi::ClearValue(1.f, 0)
        };

        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });
    }

    void DepthPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
    }

    void DepthPass::SetupSubPass(rdg::RasterSubPassBuilder& subPass, RenderScene &scene)
    {
        auto *sceneView = scene.GetSceneView(Name("MainCamera"));

        subPass.SetViewMask(0);

        subPass.AddQueue(Name("queue1"))
            .SetRasterID(Name("DepthOnly"))
            .SetView(sceneView)
            .SetLayout(layout);
    }

    struct ResolveInfo {
        float near;
        float far;
        float padding[2];
    };

    DepthResolvePass::DepthResolvePass(const RDGfxTechPtr &tech, const Name& in, const Name& out)
        : FullScreenPass(Name("DepthResolve"), tech)
    {
        colors.emplace_back(Attachment{
            rdg::RasterAttachment{out, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            in,
            rdg::ComputeView{Name("InDepth"), rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
        });

        computeResources.emplace_back(ComputeResource{
            Name("ResolveInfo"),
            rdg::ComputeView{Name("global"), rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::FS}
        });
        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(
            rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::FS, "global");
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 1, rhi::ShaderStageFlagBit::FS, "InDepth"});
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
            rhi::DescriptorType::SAMPLER, 1, 2, rhi::ShaderStageFlagBit::FS, "InDepthSampler"});

        layout = new ResourceGroupLayout();
        layout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        layout->AddNameHandler(Name("global"), {0, sizeof(ResolveInfo)});
        layout->AddNameHandler(Name("InDepth"), {1, 0});
        layout->AddNameHandler(Name("InDepthSampler"), {2, 0});

        ubo = new DynamicUniformBuffer();
        ubo->Init(sizeof(ResolveInfo));
    }

    void DepthResolvePass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        auto *sceneView = scene.GetSceneView(Name("MainCamera"));

        ResolveInfo resolveInfo = {};
        resolveInfo.near = sceneView->GetNearPlane();
        resolveInfo.far = sceneView->GetFarPlane();
        ubo->WriteT(0, resolveInfo);

        rdg.resourceGraph.ImportUBO(Name("ResolveInfo"), ubo);

        FullScreenPass::Setup(rdg, scene);
    }

    HizGenerateMip::HizGenerateMip(const RDGfxTechPtr &tech, const Name& in, const Name& out)
        : FullScreenPass(Name("DepthDownSample"), tech)
    {
        colors.emplace_back(Attachment{
            rdg::RasterAttachment{out, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            in,
            rdg::ComputeView{Name("InDepth"), rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
        });

        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 01, rhi::ShaderStageFlagBit::FS, "InDepth"});
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
            rhi::DescriptorType::SAMPLER, 1, 1, rhi::ShaderStageFlagBit::FS, "InDepthSampler"});

        layout = new ResourceGroupLayout();
        layout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        layout->AddNameHandler(Name("InDepth"), {0, 0});
        layout->AddNameHandler(Name("InDepthSampler"), {1, 0});
    }

    HizGenerator::HizGenerator(const RDGfxTechPtr &resolveTech, const RDGfxTechPtr &downTech) : technique(downTech)
    {
        depthResolve = std::make_unique<DepthResolvePass>(resolveTech, Name(FWD_DS.data()), Name("HizMipmap0"));
    }

    void HizGenerator::BuildHizPass(rdg::RenderGraph &rdg, uint32_t width, uint32_t height)
    {
        uint32_t level = rhi::GetMipLevel(width, height);
        SKY_ASSERT(level > 1);
        uint32_t downNum = level - 1;
        if (mips.size() != (level - 1)) {
            mips.resize(downNum);
        }

        auto &rsg = rdg.resourceGraph;

        std::vector<Name> names(level);
        for (uint32_t i = 0; i < level; ++i) {
            std::stringstream ss;
            ss << "HizMipmap" << i;
            rdg::GraphImageView view = {};
            view.view.subRange.baseLevel = i;
            names[i] = Name(ss.str().c_str());
            rsg.AddImageView(names[i], Name("HizDepth"), view);
        }

        uint32_t tmpWidth = width;
        uint32_t tmpHeight = height;
        for (uint32_t i = 0; i < downNum; ++i) {
            tmpWidth  = std::max(1U, tmpWidth / 2U);
            tmpHeight = std::max(1U, tmpHeight / 2U);
            if (!mips[i]) {
                mips[i] = std::make_unique<HizGenerateMip>(technique, names[i], names[i + 1]);
            }
            mips[i]->Resize(tmpWidth, tmpHeight);
        }

        depthResolve->Resize(width, height);

    }

    void HizGenerator::AddPass(RenderScenePipeline& pipeline)
    {
        pipeline.AddPass(depthResolve.get());
        for(auto &mip : mips) {
            if (mip) {
                pipeline.AddPass(mip.get());
            }
        }
    }
} // namespace sky