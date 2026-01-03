//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/Skeleton.h>

namespace sky {

    struct AnimationTick;
    class AnimationAsyncContext;

    struct AnimContext {
        AnimationAsyncContext* instance = nullptr;
    };

    struct AnimationEval : AnimContext {
        AnimPose pose;

        AnimationEval() = default;
        AnimationEval(const SkeletonPtr& skeleton)
        {
            pose.skeleton = skeleton.Get();
            pose.transforms.resize(skeleton->GetNumBones());
        }
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

    struct AnimFinalPose : AnimPose {
        AnimFinalPose() = default;

        explicit AnimFinalPose(const SkeletonPtr& inSkeleton)
        {
            SetSkeleton(inSkeleton);
        }

        explicit AnimFinalPose(const AnimPose& inPose)
            : AnimPose(inPose)
        {
            SetSkeleton(inPose.skeleton);
        }

        void SetSkeleton(const SkeletonPtr& inSkeleton)
        {
            holder = inSkeleton;
            skeleton = inSkeleton.Get();

            transforms.resize(skeleton->GetNumBones());
        }

        SkeletonPtr holder;
    };

    class AnimNode {
    public:
        AnimNode() = default;
        virtual ~AnimNode() = default;

        virtual void PreTick(const AnimationTick& tick) {}

        virtual void InitAny(const AnimContext& context) = 0;
        virtual void TickAny(const AnimLayerContext& context, float deltaTime) {}
        virtual void EvalAny(AnimationEval& context) = 0;
    };

} // namespace sky
