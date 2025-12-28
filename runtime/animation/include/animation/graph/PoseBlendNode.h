//
// Created by blues on 2025/12/14.
//

#pragma once

#include <animation/graph/AnimationNode.h>
#include <animation/core/AnimationInput.h>

namespace sky {

    class PoseBlendNodeList : public AnimNode {
    public:
        PoseBlendNodeList() = default;
        ~PoseBlendNodeList() override = default;

        void InitAsync(const AnimContext& context) override {}
        void EvalAsync(PoseContext& context, float deltaTime) override;

    private:
        std::vector<AnimNode*> poses;
        std::vector<float> desiredWeights;
        std::vector<float> cachedWeights;
    };

    class PoseBlend2Node : public AnimNode {
    public:
        PoseBlend2Node(AnimNode* a, AnimNode* b);
        ~PoseBlend2Node() override = default;

        void InitAsync(const AnimContext& context) override;
        void TickAsync(const AnimLayerContext& context, float deltaTime) override;
        void EvalAsync(PoseContext& context, float deltaTime) override;

    private:
        AnimNode* poseA;
        AnimNode* poseB;

        AnimFadeInOut fadeInOut;

        float blendedAlpha = 0.f;

        bool blendEnable = false;
        bool isARelevant = false;
        bool isBRelevant = false;
    };

} // namespace sky
