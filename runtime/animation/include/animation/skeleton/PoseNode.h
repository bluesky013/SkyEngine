//
// Created by blues on 2025/12/14.
//

#pragma once

#include <animation/graph/AnimationNode.h>
#include <animation/skeleton/Pose.h>

namespace sky {

    class AnimPoseNode;

    struct PoseContext : public AnimContext {
        Pose pose;
    };

    class AnimPoseNode : public AnimNode {
    public:
        AnimPoseNode() = default;
        ~AnimPoseNode() override = default;

        virtual void Eval(PoseContext& context, float deltaTime) {}
    };

} // namespace sky
