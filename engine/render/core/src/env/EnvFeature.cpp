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
        const auto& defaultRes = Renderer::Get()->GetDefaultResource();

        irradiance = defaultRes.textureCubeWhite;
        radiance = defaultRes.textureCubeWhite;
    }

    void EnvFeatureProcessor::SetRadiance(const RDTextureCubePtr &tex)
    {
        const auto& defaultRes = Renderer::Get()->GetDefaultResource();
        radiance = tex ? tex : defaultRes.textureCubeWhite;
    }

    void EnvFeatureProcessor::SetIrradiance(const RDTextureCubePtr &tex)
    {
        const auto& defaultRes = Renderer::Get()->GetDefaultResource();
        irradiance = tex ? tex : defaultRes.textureCubeWhite;
    }

    void EnvFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        radiance->Wait();
        irradiance->Wait();

        rdg.resourceGraph.ImportImage(Name("PrefilteredMap"), radiance->GetImage(), rhi::ImageViewType::VIEW_CUBE, rhi::AccessFlagBit::FRAGMENT_SRV);
        rdg.resourceGraph.ImportImage(Name("IrradianceMap"), irradiance->GetImage(), rhi::ImageViewType::VIEW_CUBE, rhi::AccessFlagBit::FRAGMENT_SRV);
    }

} // namespace sky