//
// Created by Zach Lee on 2023/9/16.
//

#include <render/particle/ParticleSystem.h>
#include <render/particle/ParticleModules.h>

namespace sky {

    void ParticleSystem::SetDuration(float time)
    {
        duration = time;
    }

    void ParticleSystem::Tick(float delta)
    {
        for (auto &emitter : emitters) {
            emitter->Tick(delta, currentTime);
        }
        currentTime += delta;
    }

    ParticleEmitter *ParticleSystem::AddEmitter()
    {
        return emitters.emplace_back(new ParticleEmitter).get();;
    }

} // namespace sky