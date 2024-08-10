//
// Created by blues on 2024/8/30.
//

#pragma once

#include <framework/world/Component.h>
#include <render/adaptor/assets/AnimationAsset.h>

namespace sky {
    class SerializationContext;

    class AnimationComponent : public ComponentBase {
    public:
        AnimationComponent() = default;
        ~AnimationComponent() override = default;

        COMPONENT_RUNTIME_INFO(AnimationComponent)

        static void Reflect(SerializationContext *context);

        void AddAnimation(const AnimationClip &clip);

    private:
        std::vector<AnimationAssetPtr> animations;
    };

} // namespace sky