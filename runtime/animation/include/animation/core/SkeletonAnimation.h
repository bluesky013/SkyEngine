//
// Created by Zach Lee on 2025/6/15.
//

#pragma once

#include <animation/core/Animation.h>
#include <animation/core/Skeleton.h>

namespace sky {

    class SkeletonAsyncContext;

    struct SkeletonAnimationInit : AnimationInit {
        SkeletonPtr skeleton;
    };

    class SkeletonAnimation : public Animation {
    public:
        SkeletonAnimation();
        ~SkeletonAnimation() override = default;

        void Init(const SkeletonAnimationInit& init);

        const SkeletonPtr &GetSkeleton() const { return skeleton; }
    private:

        SkeletonPtr skeleton;
        SkeletonAsyncContext* context;
    };

    class SkeletonAsyncContext : public AnimationAsyncContext {
    public:
        explicit SkeletonAsyncContext(SkeletonAnimation* anim);

    private:
        friend class SkeletonAnimation;

        SkeletonPtr skeleton;
    };

} // namespace sky
