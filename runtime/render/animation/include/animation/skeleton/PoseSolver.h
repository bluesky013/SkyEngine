//
// Created by blues on 2024/8/30.
//

#pragma once

#include <animation/skeleton/Pose.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationClip.h>

namespace sky {
    class BoneSolver {
    public:
        explicit BoneSolver(const AnimNodeChannelPtr &channel_) : channel(channel_) {} // NOLINT
        ~BoneSolver() = default;

    private:
        AnimNodeChannelPtr channel;
    };

    class PoseSolver {
    public:
        explicit PoseSolver(const PosePtr &pose_) : pose(pose_) {} // NOLINT
        ~PoseSolver() = default;

    private:
        PosePtr pose;
    };

} // namespace sky