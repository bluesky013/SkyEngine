//
// Created by Zach Lee on 2023/9/17.
//

#include <render/adaptor/components/ParticleSystemComponent.h>
#include <render/adaptor/Util.h>
#include <render/particle/ParticleFeatureProcessor.h>
#include <render/particle/ParticleModules.h>

namespace sky {

    void ParticleSystemComponent::OnTick(float time)
    {
        if (particleSystem != nullptr) {
            particleSystem->Tick(time);
        }
    }

    void ParticleSystemComponent::OnActive()
    {
        // todo: data driven.
        if (particleSystem == nullptr) {
            auto *pf = GetFeatureProcessor<ParticleFeatureProcessor>(GetRenderSceneFromGameObject(object));
            particleSystem = pf->CreateParticleSystem();

            auto *emitter = particleSystem->AddEmitter();
            emitter->RegisterPerParticleComponent<ParticleLifeTime>();
            emitter->RegisterPerParticleComponent<ParticlePosition>();

            emitter->AddSpawnModule(ParticleSpawnSimple{10.f});
            emitter->AddInitModule(ParticleInitAlignedBox{VEC3_ZERO, VEC3_ONE});
            emitter->AddSolverModule(ParticlePositionSolver{});
        }
    }

    void ParticleSystemComponent::OnDestroy()
    {

    }
} // namespace sky