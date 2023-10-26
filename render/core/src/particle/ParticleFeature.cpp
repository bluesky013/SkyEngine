//
// Created by Zach Lee on 2023/9/17.
//

#include <render/particle/ParticleFeature.h>
#include <render/particle/ParticleFeatureProcessor.h>
#include <render/Renderer.h>

namespace sky {

    void ParticleFeature::Init()
    {
        Renderer::Get()->RegisterRenderFeature<ParticleFeatureProcessor>();
    }
} // namespace sky