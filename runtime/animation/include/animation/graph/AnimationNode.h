//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationConfigs.h>

namespace sky {

    class AnimationAsyncContext;

    struct AnimContext {
        AnimationAsyncContext* instance = nullptr;
    };

    struct PoseContext : AnimContext {
        AnimPose pose;
    };

    struct AnimLayerContext : AnimContext {
        float weight = 1.f;

        AnimLayerContext MakeContext(float inWeight) const
        {
            AnimLayerContext context(*this);
            context.weight = weight * inWeight;
            return context;
        }
    };

    class AnimNode {
    public:
        AnimNode() = default;
        virtual ~AnimNode() = default;

        virtual void InitAny(const AnimContext& context) = 0;
        virtual void TickAny(const AnimLayerContext& context, float deltaTime) {}
        virtual void EvalAny(PoseContext& context, float deltaTime) = 0;
    };

} // namespace sky
