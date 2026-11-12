#pragma once

#include <animation/IAnimationInstanceControl.h>
#include <vector>

namespace sky {
    class SerializationContext;
}

namespace sky::animation {
    struct AnimationParameterOverride {
        Uuid  parameter;
        bool  isBoolean                                            = false;
        bool  booleanValue                                         = false;
        float floatValue                                           = 0.f;
        bool  operator==(const AnimationParameterOverride &) const = default;
    };
    struct AnimationComponentData {
        std::vector<AnimationParameterOverride> parameters;
        AnimationMode                           mode = AnimationMode::Clip;
        Uuid                                    skeleton;
        Uuid                                    clip;
        Uuid                                    graph;
        bool                                    autoplay                                         = true;
        bool                                    loop                                             = true;
        float                                   playRate                                         = 1.f;
        AnimationRootPolicy                     rootPolicy                                       = AnimationRootPolicy::KeepInPose;
        bool                                    operator==(const AnimationComponentData &) const = default;
    };
    void RegisterAnimationComponentData(SerializationContext *context);
} // namespace sky::animation
