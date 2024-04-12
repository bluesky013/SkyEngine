//
// Created by Zach Lee on 2023/9/17.
//

#pragma once

#include <framework/world/Component.h>
#include <render/particle/ParticleSystem.h>

namespace sky {

    class ParticleSystemComponent : public Component {
    public:
        ParticleSystemComponent() = default;
        ~ParticleSystemComponent() override = default;

        TYPE_RTTI_WITH_VT(ParticleSystemComponent)

        void OnTick(float time) override;
        void OnActive() override;
        void OnDestroy() override;

    private:
        ParticleSystem *particleSystem;
    };

} // namespace sky
