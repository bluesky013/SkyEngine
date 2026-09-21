//
// Created by Zach Lee on 2025/6/12.
//

#pragma once

#include <animation/core/AnimationPose.h>
#include <animation/core/Skeleton.h>

namespace sky {

    struct AnimationTick;
    class AnimationAsyncContext;
    class AnimationPlanBuilder;
    struct AnimPlanLowerInfo;

    struct AnimContext {
        AnimationAsyncContext* instance = nullptr;
    };

    struct AnimationEval : AnimContext {
        AnimPose pose;
        bool sampleRootMotion = false;
        Transform rootMotionDelta;

        AnimationEval() = default;
        AnimationEval(const SkeletonPtr& skeleton)
        {
            pose.skeleton = skeleton.Get();
            pose.transforms = skeleton->GetRefPos()->transforms;
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

        /**
         * Optional lowering into the data-oriented evaluation plan. Nodes that do not
         * implement this cannot be compiled and are reported by the compiler.
         */
        virtual bool LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot)
        {
            (void)builder;
            (void)info;
            (void)outSlot;
            return false;
        }
    };

} // namespace sky
