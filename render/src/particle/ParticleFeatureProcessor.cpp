//
// Created by Zach Lee on 2023/9/16.
//

#include <render/particle/ParticleFeatureProcessor.h>
#include <render/particle/ParticleModules.h>

namespace sky {

    void ParticleFeatureProcessor::Tick(float time)
    {

    }

    void ParticleFeatureProcessor::Render()
    {

    }

    ParticleSystem *ParticleFeatureProcessor::CreateParticleSystem()
    {
        auto *ps = new ParticleSystem();
        return particleSystems.emplace_back(ps).get();
    }

    void ParticleFeatureProcessor::RemoveParticleSystem(ParticleSystem *particle)
    {
        particleSystems.remove_if([particle](const auto &val) {
            return particle == val.get();
        });
    }

} // namespace sky