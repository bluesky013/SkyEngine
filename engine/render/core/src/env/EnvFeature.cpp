//
// Created by blues on 2025/2/17.
//

#include <render/env/EnvFeature.h>
#include <render/Renderer.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    void EnvFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<EnvFeatureProcessor>();
    }

    EnvFeatureProcessor::EnvFeatureProcessor(RenderScene *scn) : IFeatureProcessor(scn)
    {
    }

    void EnvFeatureProcessor::SetRadiance(const RDTextureCubePtr &tex)
    {
        radiance = tex;
    }

    void EnvFeatureProcessor::SetIrradiance(const RDTextureCubePtr &tex)
    {
        irradiance = tex;
    }

    void EnvFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        const auto &defaultRes = Renderer::Get()->GetDefaultResource();

        bool enableIBL = false;

        if (irradiance != nullptr && irradiance->IsReady()) {
            rdg.resourceGraph.ImportImage(Name("IrradianceMap"), irradiance->GetImage(), rhi::ImageViewType::VIEW_CUBE,
                                          rhi::AccessFlagBit::FRAGMENT_SRV);
            enableIBL = true;
        }
        else {
            rdg.resourceGraph.ImportImage(Name("IrradianceMap"), defaultRes.textureCubeWhite->GetImage(), rhi::ImageViewType::VIEW_CUBE,
                                          rhi::AccessFlagBit::FRAGMENT_SRV);
        }

        if (radiance != nullptr && radiance->IsReady()) {
            rdg.resourceGraph.ImportImage(Name("PrefilteredMap"), radiance->GetImage(), rhi::ImageViewType::VIEW_CUBE,
                                          rhi::AccessFlagBit::FRAGMENT_SRV);
            enableIBL = true;
        } else {
            rdg.resourceGraph.ImportImage(Name("PrefilteredMap"), defaultRes.textureCubeWhite->GetImage(), rhi::ImageViewType::VIEW_CUBE,
                                          rhi::AccessFlagBit::FRAGMENT_SRV);
        }

        rdg.pipelineKey[Name("ENABLE_IBL")] = enableIBL ? 1 : 0;
    }

} // namespace sky