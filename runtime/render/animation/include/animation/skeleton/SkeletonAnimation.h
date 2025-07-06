//
// Created by Zach Lee on 2025/6/15.
//

#pragma once

#include <animation/core/Animation.h>
#include <animation/skeleton/Skeleton.h>

namespace sky {

    class SkeletonAnimation : public Animation {
    public:
        explicit SkeletonAnimation(const SkeletonPtr& skeleton);
        ~SkeletonAnimation() override = default;

    private:
        void Tick(float deltaTime) override;

        SkeletonPtr skeleton;
    };

} // namespace sky
