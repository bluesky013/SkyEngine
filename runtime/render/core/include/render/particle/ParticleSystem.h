//
// Created by Zach Lee on 2023/9/16.
//

#pragma once

#include <render/particle/ParticleEmitter.h>

namespace sky {
    class ParticleSystem {
    public:
        ParticleSystem() = default;
        ~ParticleSystem() = default;

        void SetDuration(float time);
        void Tick(float time);

        ParticleEmitter *AddEmitter();

    private:
        float duration = 0.f;
        float currentTime = 0.f;

        std::list<std::unique_ptr<ParticleEmitter>> emitters;
    };

} // namespace sky