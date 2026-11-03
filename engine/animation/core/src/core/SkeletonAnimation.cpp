//
// Created by Zach Lee on 2025/6/15.
//

#include <animation/core/SkeletonAnimation.h>
#include <animation/core/AnimationClip.h>

namespace sky {

    SkeletonAsyncContext::SkeletonAsyncContext(SkeletonAnimation* anim)
        : AnimationAsyncContext(anim)
    {
    }

    SkeletonAnimation::SkeletonAnimation()
        : Animation(new SkeletonAsyncContext(this))
        , context(static_cast<SkeletonAsyncContext*>(asyncContext.get()))
    {
    }

    void SkeletonAnimation::Init(const SkeletonAnimationInit& init)
    {
        Animation::Init(init);
        context->skeleton = init.skeleton;
    }

    void SkeletonAnimation::EvalAny(AnimationEval &eval)
    {
        asyncContext->EvalAny(eval);
    }

    const SkeletonPtr &SkeletonAnimation::GetSkeleton() const
    {
        return context->skeleton;
    }

} // namespace sky
