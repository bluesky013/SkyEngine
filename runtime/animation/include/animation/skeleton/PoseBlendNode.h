//
// Created by blues on 2025/12/14.
//

#pragma once

#include <animation/skeleton/PoseNode.h>

namespace sky {

    class PoseBlendNode : public AnimPoseNode {
    public:
        PoseBlendNode() = default;
        ~PoseBlendNode() override = default;

        void Eval(PoseContext& context, float deltaTime) override;

    private:
        std::vector<AnimPoseNode*> poses;
        std::vector<float> desiredWeights;
        std::vector<float> cachedWeights;
    };

} // namespace sky
