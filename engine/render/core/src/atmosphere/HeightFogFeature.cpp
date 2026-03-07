//
// Created by SkyEngine on 2025/3/7.
//

#include <render/atmosphere/HeightFogFeature.h>
#include <render/Renderer.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    void HeightFogFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<HeightFogFeatureProcessor>();
    }

    HeightFogFeatureProcessor::HeightFogFeatureProcessor(RenderScene *scn)
        : IFeatureProcessor(scn)
    {
        params.fogColor          = Vector4(0.7f, 0.8f, 0.9f, 0.f);
        params.inscatterColor    = Vector4(0.9f, 0.85f, 0.7f, 0.f);
        params.fogDensity        = 0.02f;
        params.heightFalloff     = 0.15f;
        params.baseHeight        = 0.f;
        params.maxHeight         = 50.f;
        params.startDistance     = 0.f;
        params.inscatterExponent = 8.f;
        params.clipYSign         = RHI::Get()->GetDevice()->GetConstants().flipY ? 1.f : -1.f;
        params.padding0          = 0.f;

        ubo = new UniformBuffer();
        ubo->Init(sizeof(HeightFogParams));
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetFogColor(const Vector4 &color)
    {
        params.fogColor = color;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetInscatterColor(const Vector4 &color)
    {
        params.inscatterColor = color;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetFogDensity(float density)
    {
        params.fogDensity = density;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetHeightFalloff(float falloff)
    {
        params.heightFalloff = falloff;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetBaseHeight(float height)
    {
        params.baseHeight = height;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetMaxHeight(float height)
    {
        params.maxHeight = height;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::SetStartDistance(float distance)
    {
        params.startDistance = distance;
        ubo->WriteT(0, params);
    }

    void HeightFogFeatureProcessor::Render(rdg::RenderGraph &rdg)
    {
        rdg.resourceGraph.ImportUBO(Name("HeightFogParamsBuffer"), ubo);
    }

} // namespace sky
