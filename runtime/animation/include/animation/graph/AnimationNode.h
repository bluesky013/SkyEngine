//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <animation/core/AnimationPose.h>
#include <animation/core/AnimationConfigs.h>

namespace sky {

    class Animation;

    struct AnimContext {
        Animation* animInstance = nullptr;
    };

    struct PoseContext : public AnimContext {
        AnimPose pose;
    };

    struct AnimLayerContext : public AnimContext {
        float weight = 1.f;

        AnimLayerContext MakeContext(float inWeight) const
        {
            AnimLayerContext context(*this);
            context.weight = weight * inWeight;
            return context;
        }
    };

    class AnimNode : public RefObject {
    public:
        AnimNode() = default;
        ~AnimNode() override = default;

        virtual void InitAsync(const AnimContext& context) = 0;
        virtual void TickAsync(const AnimLayerContext& context, float deltaTime) {}
        virtual void EvalAsync(PoseContext& context, float deltaTime) = 0;
    };

} // namespace sky
