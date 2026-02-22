//
// Created by blues on 2024/8/30.
//

#include <render/adaptor/animation/AnimationComponent.h>
#include <framework/serialization/SerializationContext.h>

namespace sky {

    void AnimationComponent::Reflect(SerializationContext *context)
    {
        context->Register<AnimationComponent>("AnimationComponent");
    }

} // namespace sky