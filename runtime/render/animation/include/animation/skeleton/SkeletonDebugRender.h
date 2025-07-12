//
// Created by Zach Lee on 2025/7/9.
//

#pragma once

#include <animation/skeleton/Skeleton.h>

namespace sky {

    class SkeletonDebugRender {
    public:
        SkeletonDebugRender() = default;
        ~SkeletonDebugRender() = default;

        void SetSkeleton(const SkeletonPtr &skeleton_) { skeleton = skeleton_; }

    private:
        SkeletonPtr skeleton;
    };

} // namespace sky
