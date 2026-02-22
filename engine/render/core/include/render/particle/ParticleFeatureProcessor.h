//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include <render/FeatureProcessor.h>
#include <render/particle/ParticleSystem.h>

namespace sky {

    class ParticleFeatureProcessor : public IFeatureProcessor {
    public:
        explicit ParticleFeatureProcessor(RenderScene *scene) : IFeatureProcessor(scene) {}
        ~ParticleFeatureProcessor() override = default;

        void Tick(float time) override;
        void Render(rdg::RenderGraph &rdg) override;

        ParticleSystem *CreateParticleSystem();
        void RemoveParticleSystem(ParticleSystem *mesh);

    private:
        std::list<std::unique_ptr<ParticleSystem>> particleSystems;
    };

} // namespace sky
